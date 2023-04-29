const path = require('path');
process.env.ONNX_ROOT = path.join(__dirname, '..', 'build', 'Release');

const prism = require('prism-media');
const fs = require('fs');

const {AprilAsrNative, Model, Session} = require('../src/index.js');

// Example usage:

AprilAsrNative.init(1);

const model = new Model('C:\\Users\\smluc\\Downloads\\aprilv0_en-us.april');
console.log('Name: ' + model.getName())
console.log('Description: ' + model.getDescription())
console.log('Language: ' + model.getLanguage())
console.log('Sample rate: ' + model.getSampleRate())

const session = new Session(model, (result, tokens) => {
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
    '-ar', model.getSampleRate(),
    '-sample_fmt', 's16',
    '-ac', '1'
  ],
});

fileStream.pipe(transcoder);

transcoder.on('data', (chunk) => {
  // Feed the audio data
  session.feedPCM16(chunk);
});

transcoder.on('end', () => {
  // Flush to finish off

  session.flush();
  session.close();
});