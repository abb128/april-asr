#!/usr/bin/env python3
# flake8: noqa
#
# Copyright 2021-2022 Xiaomi Corporation (Author: Fangjun Kuang, Zengwei Yao)
# Author: abb128
#
# See https://github.com/k2-fsa/icefall/blob/b3920e5/LICENSE for clarification
# regarding multiple authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script converts several saved checkpoints
# to a single one using model averaging.
"""

Usage:

Export to ONNX format

./lstm_transducer_stateless2/export-april.py \
  --exp-dir ./lstm_transducer_stateless2/exp \
  --bpe-model data/lang_bpe_500/bpe.model \
  --epoch 20 \
  --avg 10 \
  --name "Some Model" \
  --language "en-us" \
  --description "This is an example model"

It will generate the following file in the given `exp_dir`.
    - (name)_(language).april

This model may be loaded and used with libapril
"""

import argparse
import logging
from pathlib import Path
from typing import Tuple
import unicodedata
import re
from io import BytesIO
import struct

import sentencepiece as spm
import torch
import torch.nn as nn
from scaling_converter import convert_scaled_to_non_scaled
from train import add_model_arguments, get_params, get_transducer_model

from icefall.checkpoint import (
    average_checkpoints,
    average_checkpoints_with_averaged_model,
    find_checkpoints,
    load_checkpoint,
)
from icefall.utils import str2bool


def get_parser():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        "--epoch",
        type=int,
        default=28,
        help="""It specifies the checkpoint to use for averaging.
        Note: Epoch counts from 0.
        You can specify --avg to use more checkpoints for model averaging.""",
    )

    parser.add_argument(
        "--iter",
        type=int,
        default=0,
        help="""If positive, --epoch is ignored and it
        will use the checkpoint exp_dir/checkpoint-iter.pt.
        You can specify --avg to use more checkpoints for model averaging.
        """,
    )

    parser.add_argument(
        "--avg",
        type=int,
        default=15,
        help=(
            "Number of checkpoints to average. Automatically select "
            "consecutive checkpoints before the checkpoint specified by "
            "'--epoch' and '--iter'"
        ),
    )

    parser.add_argument(
        "--use-averaged-model",
        type=str2bool,
        default=True,
        help=(
            "Whether to load averaged model. Currently it only supports "
            "using --epoch. If True, it would decode with the averaged model "
            "over the epoch range from `epoch-avg` (excluded) to `epoch`."
            "Actually only the models with epoch number of `epoch-avg` and "
            "`epoch` are loaded for averaging. "
        ),
    )

    parser.add_argument(
        "--exp-dir",
        type=str,
        default="pruned_transducer_stateless3/exp",
        help="""It specifies the directory where all training related
        files, e.g., checkpoints, log, etc, are saved
        """,
    )

    parser.add_argument(
        "--bpe-model",
        type=str,
        default="data/lang_bpe_500/bpe.model",
        help="Path to the BPE model",
    )

    parser.add_argument(
        "--name",
        type=str,
        default="Untitled Model",
        help="Specify a human-readable name for your model",
    )

    parser.add_argument(
        "--description",
        type=str,
        default="No description added.",
        help="Specify information about your model.",
    )

    parser.add_argument(
        "--language",
        type=str,
        default="en-us",
        help="Specify the IETF language tag for your model.",
    )

    parser.add_argument(
        "--context-size",
        type=int,
        default=2,
        help="The context size in the decoder. 1 means bigram; 2 means tri-gram",
    )

    add_model_arguments(parser)

    return parser


def slugify(value, allow_unicode=False):
    """
    Taken from https://github.com/django/django/blob/master/django/utils/text.py
    Convert to ASCII if 'allow_unicode' is False. Convert spaces or repeated
    dashes to single dashes. Remove characters that aren't alphanumerics,
    underscores, or hyphens. Convert to lowercase. Also strip leading and
    trailing whitespace, dashes, and underscores.
    """
    value = str(value)
    if allow_unicode:
        value = unicodedata.normalize('NFKC', value)
    else:
        value = unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode('ascii')
    value = re.sub(r'[^\w\s-]', '', value.lower())
    return re.sub(r'[-\s]+', '-', value).strip('-_')


class MergedEncoder(nn.Module):
    """
    This combines the encoder and joiner to provide a simplified model where
    the encoder_out is pre-projected according to the joiner.
    """

    def __init__(self, encoder: nn.Module, encoder_proj: nn.Module) -> None:
        super().__init__()
        self.encoder = encoder
        self.encoder_proj = encoder_proj

    def forward(
        self, x: torch.Tensor, h: torch.Tensor, c: torch.Tensor
    ) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        warmup = 1.0
        x_lens = torch.tensor([9], dtype=torch.int64)

        x, _, new_states = self.encoder(x, x_lens, (h, c), warmup)
        x = self.encoder_proj(x)

        return x, new_states[0], new_states[1]


class MergedDecoder(nn.Module):
    """
    This combines the decoder and joiner to provide a simplified model where
    the decoder_out is pre-projected according to the joiner.
    """

    def __init__(self, decoder: nn.Module, decoder_proj: nn.Module) -> None:
        super().__init__()
        self.decoder = decoder
        self.decoder_proj = decoder_proj

    def forward(self, context: torch.Tensor) -> torch.Tensor:
        need_pad = False  # Always False, so we can use torch.jit.trace() here

        decoder_out = self.decoder(context, need_pad)
        decoder_out = self.decoder_proj(decoder_out)

        return decoder_out


def export_model_onnx(model: nn.Module, sp, opset_version: int = 11) -> Tuple[BytesIO, BytesIO, BytesIO, BytesIO]:
    """Export the given model to ONNX format.
    This exports the model as 3 networks:
        - encoder.onnx, which combines the encoder and joiner's encoder_proj
        - decoder.onnx, which combines the decoder and joiner's decoder_proj
        - joiner.onnx, which takes encoder and decoder outputs


    The encoder network has 3 inputs:
        - x: mel features, a tensor of shape (N, T, C); dtype is torch.float32
        - h: hidden state, a tensor of shape (num_layers, N, proj_size)
        - c: cell state, a tensor of shape (num_layers, N, hidden_size)
    and has 3 outputs:
        - encoder_out: a tensor of shape (N, T', joiner_dim)
        - next_h: a tensor of shape (num_layers, N, proj_size)
        - next_c: a tensor of shape (num_layers, N, hidden_size)

    h0 and c0 should be initialized to zeros in the beginning. The outputs
    next_h0 and next_c0 should be provided as h0 and c0 inputs in the
    subsequent call.

    Note: The warmup argument is fixed to 1.


    The decoder network has 1 inputs:
        - context: a torch.int64 tensor of shape (N, decoder_model.context_size)
    and has one output:
        - decoder_out: a tensor of shape (N, 1, joiner_dim)


    The joiner network has 2 inputs:
        - encoder_out: a tensor of shape (N, 1, joiner_dim)
        - decoder_out: a tensor of shape (N, 1, joiner_dim)
    and has one output:
        - logit: a tensor of shape (N, vocab_size)

    Args:
      model:
        The input model
      out_path:
        The path to save the exported ONNX models.
      opset_version:
        The opset version to use.
    Returns:
      encoder_b
      decoder_b
      joiner_b
      params_b
    """
    encoder_b = BytesIO()
    decoder_b = BytesIO()
    joiner_b  = BytesIO()
    params_b  = BytesIO()

    onnx_encoder = MergedEncoder(model.encoder, model.joiner.encoder_proj)
    onnx_decoder = MergedDecoder(model.decoder, model.joiner.decoder_proj)
    onnx_encoder.eval()
    onnx_decoder.eval()

    N = 1
    SEGMENT_SIZE = 9
    MEL_FEATURES = 80

    # Export encoder
    x = torch.zeros(N, SEGMENT_SIZE, MEL_FEATURES, dtype=torch.float32)
    h = torch.rand(model.encoder.num_encoder_layers, N, model.encoder.d_model)
    c = torch.rand(model.encoder.num_encoder_layers, N, model.encoder.rnn_hidden_size)
    torch.onnx.export(
        onnx_encoder,  # use torch.jit.trace() internally
        (x, h, c),
        encoder_b,
        verbose=False,
        opset_version=opset_version,
        input_names=["x", "h", "c"],
        output_names=["encoder_out", "next_h", "next_c"]
    )
    logging.info(f"Serialized encoder")

    # Export decoder
    context = torch.zeros(N, model.decoder.context_size, dtype=torch.int64)
    torch.onnx.export(
        onnx_decoder,  # use torch.jit.trace() internally
        (context),
        decoder_b,
        verbose=False,
        opset_version=opset_version,
        input_names=["context"],
        output_names=["decoder_out"]
    )
    logging.info(f"Serialized decoder")

    # Export joiner
    encoder_out, _, _ = onnx_encoder(x, h, c)
    decoder_out = onnx_decoder(context)

    project_input = False

    torch.onnx.export(
        model.joiner,  # use torch.jit.trace() internally
        (encoder_out, decoder_out, project_input),
        joiner_b,
        verbose=False,
        opset_version=opset_version,
        input_names=["encoder_out", "decoder_out"],
        output_names=["logits"]
    )
    logging.info(f"Serialized joiner")


    SAMPLERATE = 16000
    SEGMENT_STEP = 4
    FRAME_SHIFT_MS = 10
    FRAME_LENGTH_MS = 25
    ROUND_POW2 = True
    MEL_LOW = 20
    MEL_HIGH = 0
    SNIP_EDGES = False

    params_b.write(b"PARAMS\0\0")
    params_b.write(struct.pack("<i", N))
    params_b.write(struct.pack("<i", SEGMENT_SIZE))
    params_b.write(struct.pack("<i", SEGMENT_STEP))
    params_b.write(struct.pack("<i", MEL_FEATURES))
    params_b.write(struct.pack("<i", SAMPLERATE))

    params_b.write(struct.pack("<i", FRAME_SHIFT_MS))
    params_b.write(struct.pack("<i", FRAME_LENGTH_MS))
    params_b.write(struct.pack("<i", ROUND_POW2))
    params_b.write(struct.pack("<i", MEL_LOW))
    params_b.write(struct.pack("<i", MEL_HIGH))
    params_b.write(struct.pack("<i", SNIP_EDGES))

    params_b.write(struct.pack("<i", sp.get_piece_size()))
    params_b.write(struct.pack("<i", sp.piece_to_id("<blk>")))

    # write sentence pieces
    print("write ",sp.get_piece_size(), " pieces")
    for i in range(sp.get_piece_size()):
        piece = sp.id_to_piece(i).replace("\u2581", " ").encode("utf-8")
        params_b.write(struct.pack("<i", len(piece)))
        params_b.write(piece)


    logging.info(f"Serialized params")

    return (encoder_b, decoder_b, joiner_b, params_b)


def export_model(
    model: nn.Module,
    sp,
    out_path: str,
    name: str = "Untitled",
    description: str = "No description",
    language: str = "en-us"
) -> None:
    encoder_out, decoder_out, joiner_out, params_out = export_model_onnx(model, sp)

    NUM_NETWORKS = 3
    networks = [encoder_out, decoder_out, joiner_out]

    VERSION = 1
    MODEL_KIND = 1

    header = BytesIO()


    language = language.encode("utf-8").ljust(8, b"\0")
    if len(language) > 8:
        raise RuntimeError("Language string may not be longer than 8 characters")
    
    header.write(language)

    name = name.encode("utf-8")
    header.write(struct.pack("<Q", len(name)))
    header.write(name)

    description = description.encode("utf-8")
    header.write(struct.pack("<Q", len(description)))
    header.write(description)

    header.write(struct.pack("<i", MODEL_KIND))

    # don't know offsets yet, so write zeros
    PARAMS_OFFSETS = header.tell()
    header.write(struct.pack("<Q", 0))
    header.write(struct.pack("<Q", params_out.tell()))

    NETWORK_OFFSETS = [0] * NUM_NETWORKS
    header.write(struct.pack("<Q", NUM_NETWORKS))
    for i in range(NUM_NETWORKS):
        NETWORK_OFFSETS[i] = header.tell()
        header.write(struct.pack("<Q", 0))
        header.write(struct.pack("<Q", networks[i].tell()))


    with open(out_path, "wb") as f:
        f.write(b"APRILMDL")
        f.write(struct.pack("<i", VERSION))
        f.write(struct.pack("<Q", header.tell()))

        HEADER_START = f.tell()
        f.write(header.getbuffer())

        network_offsets = [0] * NUM_NETWORKS
        for i, network in enumerate([encoder_out, decoder_out, joiner_out]):
            network_offsets[i] = f.tell()
            f.write(network.getbuffer())
        
        params_offset = f.tell()
        f.write(params_out.getbuffer())

        f.seek(HEADER_START + PARAMS_OFFSETS)
        f.write(struct.pack("<Q", params_offset))

        for i in range(NUM_NETWORKS):
            f.seek(HEADER_START + NETWORK_OFFSETS[i])
            f.write(struct.pack("<Q", network_offsets[i]))




@torch.no_grad()
def main():
    args = get_parser().parse_args()
    args.exp_dir = Path(args.exp_dir)

    params = get_params()
    params.update(vars(args))

    device = torch.device("cpu")
    if torch.cuda.is_available():
        device = torch.device("cuda", 0)

    logging.info(f"device: {device}")

    sp = spm.SentencePieceProcessor()
    sp.load(params.bpe_model)

    # <blk> is defined in local/train_bpe_model.py
    params.blank_id = sp.piece_to_id("<blk>")
    params.vocab_size = sp.get_piece_size()

    logging.info(params)

    logging.info("About to create model")
    model = get_transducer_model(params, enable_giga=False)

    num_param = sum([p.numel() for p in model.parameters()])
    logging.info(f"Number of model parameters: {num_param}")

    if not params.use_averaged_model:
        if params.iter > 0:
            filenames = find_checkpoints(params.exp_dir, iteration=-params.iter)[
                : params.avg
            ]
            if len(filenames) == 0:
                raise ValueError(
                    f"No checkpoints found for --iter {params.iter}, --avg {params.avg}"
                )
            elif len(filenames) < params.avg:
                raise ValueError(
                    f"Not enough checkpoints ({len(filenames)}) found for"
                    f" --iter {params.iter}, --avg {params.avg}"
                )
            logging.info(f"averaging {filenames}")
            model.to(device)
            model.load_state_dict(
                average_checkpoints(filenames, device=device),
                strict=False,
            )
        elif params.avg == 1:
            load_checkpoint(f"{params.exp_dir}/epoch-{params.epoch}.pt", model)
        else:
            start = params.epoch - params.avg + 1
            filenames = []
            for i in range(start, params.epoch + 1):
                if i >= 1:
                    filenames.append(f"{params.exp_dir}/epoch-{i}.pt")
            logging.info(f"averaging {filenames}")
            model.to(device)
            model.load_state_dict(
                average_checkpoints(filenames, device=device),
                strict=False,
            )
    else:
        if params.iter > 0:
            filenames = find_checkpoints(params.exp_dir, iteration=-params.iter)[
                : params.avg + 1
            ]
            if len(filenames) == 0:
                raise ValueError(
                    f"No checkpoints found for --iter {params.iter}, --avg {params.avg}"
                )
            elif len(filenames) < params.avg + 1:
                raise ValueError(
                    f"Not enough checkpoints ({len(filenames)}) found for"
                    f" --iter {params.iter}, --avg {params.avg}"
                )
            filename_start = filenames[-1]
            filename_end = filenames[0]
            logging.info(
                "Calculating the averaged model over iteration checkpoints"
                f" from {filename_start} (excluded) to {filename_end}"
            )
            model.to(device)
            model.load_state_dict(
                average_checkpoints_with_averaged_model(
                    filename_start=filename_start,
                    filename_end=filename_end,
                    device=device,
                ),
                strict=False,
            )
        else:
            assert params.avg > 0, params.avg
            start = params.epoch - params.avg
            assert start >= 1, start
            filename_start = f"{params.exp_dir}/epoch-{start}.pt"
            filename_end = f"{params.exp_dir}/epoch-{params.epoch}.pt"
            logging.info(
                "Calculating the averaged model over epoch range from "
                f"{start} (excluded) to {params.epoch}"
            )
            model.to(device)
            model.load_state_dict(
                average_checkpoints_with_averaged_model(
                    filename_start=filename_start,
                    filename_end=filename_end,
                    device=device,
                ),
                strict=False,
            )

    model.to("cpu")
    model.eval()

    logging.info("Exporting model")
    convert_scaled_to_non_scaled(model, inplace=True, is_onnx=True)
    
    out_path = params.exp_dir / (slugify(params.name + "_" + params.language) + ".april")
    export_model(model, sp, out_path, name=params.name, description=params.description, language=params.language)

    logging.info(f"Exported to {out_path}")


if __name__ == "__main__":
    formatter = "%(asctime)s %(levelname)s [%(filename)s:%(lineno)d] %(message)s"

    logging.basicConfig(format=formatter, level=logging.INFO)
    main()
