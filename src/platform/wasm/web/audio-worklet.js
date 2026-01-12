/**
 * Audio Worklet Processor for NanoBoyAdvance
 * This file is loaded by the AudioWorklet API for low-latency audio processing.
 * 
 * Note: For maximum browser compatibility, we use ScriptProcessorNode in main.js
 * This AudioWorklet implementation is provided for future use with better browser support.
 */

class NBAudioProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.samples = [];
    this.port.onmessage = (event) => {
      if (event.data.type === 'samples') {
        // Receive samples from main thread
        this.samples.push(...event.data.data);
      }
    };
  }

  process(inputs, outputs, parameters) {
    const output = outputs[0];
    const left = output[0];
    const right = output[1];

    if (!left || !right) return true;

    for (let i = 0; i < left.length; i++) {
      if (this.samples.length >= 2) {
        left[i] = this.samples.shift() / 32768;
        right[i] = this.samples.shift() / 32768;
      } else {
        left[i] = 0;
        right[i] = 0;
      }
    }

    return true;
  }
}

registerProcessor('nba-audio-processor', NBAudioProcessor);
