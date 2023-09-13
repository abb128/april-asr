import * as path from 'path';
process.env.ONNX_ROOT = path.join(__dirname, '..', 'build', 'Release');
process.env.APRIL_ASR_ROOT = path.join(__dirname, '..', 'build', 'Release');

import { AprilAsrNative, Model, Session } from '../build/src/index';
import * as prism from 'prism-media';
import * as fs from 'fs';
import * as readline from 'readline';

function Run(modelPath: string, wavFilePath: string) {
  AprilAsrNative.init(1);

  const model = new Model(modelPath);
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
  
  const fileStream = fs.createReadStream(wavFilePath);
  
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
}

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});

function question(prompt: string): Promise<string> {
	return new Promise(function(resolve, reject) {
		rl.question(prompt, (answer: string) => {
      resolve(answer);
			rl.pause();
		});
	});
}

// Main
(async function() {
  let modelPath: string = '';
  let wavFilePath: string = '';

  if (process.argv.length >= 4) {
    modelPath = process.argv[2];
    wavFilePath = process.argv[3];
  } else {
    modelPath = await question('Enter Model Path: ');
    wavFilePath = await question('Enter WAV File Path: ');
  }

  Run(modelPath, wavFilePath);
  
  rl.close();
})();