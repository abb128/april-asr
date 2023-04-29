import * as path from 'path';
process.env.ONNX_ROOT = path.join(__dirname, '..', 'build', 'Release');

import { AprilAsrNative, Model, Session } from '../build/src/index';
import * as prism from 'prism-media';
import * as fs from 'fs';

AprilAsrNative.init(1);

const model = new Model('C:\\Users\\smluc\\Downloads\\aprilv0_en-us.april');
console.log('Name: ' + model.getName())
console.log('Description: ' + model.getDescription())
console.log('Language: ' + model.getLanguage())
console.log('Sample rate: ' + model.getSampleRate())

const session = new Session(model, (result: number, tokens) => {
  if (result == 2) {
    console.log(tokens.reduce((acc, token) => acc + token.token_str, ''))
  }
}, false, false, 'Samuel');

// Feed audio stream

const fileStream = fs.createReadStream("C:\\Users\\smluc\\Downloads\\zoo.wav");

const transcoder = new prism.FFmpeg({
  args: [
    '-i', 'pipe:0',
    '-f', 's16le',
    '-ar', `${model.getSampleRate()}`,
    '-sample_fmt', 's16',
    '-ac', '1'
  ],
});

// @ts-ignore
fileStream.pipe(transcoder);

// @ts-ignore
transcoder.on('data', (chunk) => {
  // Feed the audio data
  session.feedPCM16(chunk);
});

// @ts-ignore
transcoder.on('end', () => {
  // Flush to finish off

  session.flush();
  session.close();
});