"""
Example of a program that uses april_asr to perform speech recognition on a
file
"""

from typing import List
import sys
import librosa
import april_asr as april

def example_handler(result_type: april.Result, tokens: List[april.Token]):
    """Simple handler that concatenates all tokens and prints it"""
    prefix = "."
    if result_type == april.Result.FINAL_RECOGNITION:
        prefix = "@"
    elif result_type == april.Result.PARTIAL_RECOGNITION:
        prefix = "-"

    string = ""
    for token in tokens:
        string += token.token

    print(f"{prefix}{string}")

def run(model_path: str, wav_file_path: str) -> None:
    """Creates a model and session, and performs recognition on the given file"""
    # Load the model
    model = april.Model(model_path)

    # Print the metadata
    print("Name: " + model.get_name())
    print("Description: " + model.get_description())
    print("Language: " + model.get_language())

    # Create a session
    session = april.Session(model, example_handler)

    # Read the audio file, works with any audio filetype librosa supports
    data, _ = librosa.load(wav_file_path, sr=model.get_sample_rate(), mono=True)
    data = (data * 32767).astype("short").astype("<u2").tobytes()

    # Feed the audio data
    session.feed_pcm16(data)

    # Flush to finish off
    session.flush()

def main():
    """Checks the given arguments and prints usage or calls the run function"""
    # Parse arguments
    args = sys.argv
    if len(args) != 3:
        print("Usage: " + args[0] + " /path/to/model.april /path/to/file.wav")
    else:
        run(args[1], args[2])

if __name__ == "__main__":
    main()
