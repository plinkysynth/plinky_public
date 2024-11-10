class CustomProcessor extends AudioWorkletProcessor {
    static get parameterDescriptors() {
        return [
            { name: 'gain', defaultValue: 1.0, minValue: 0.0, maxValue: 2000.0 },
            // Add parameters for the 8 biquad filters here
            ...Array.from({ length: 8 }, (_, index) => [
                { name: `b0_${index}`, defaultValue: 1.0 },
                { name: `b1_${index}`, defaultValue: 0.0 },
                { name: `b2_${index}`, defaultValue: 0.0 },
                { name: `a1_${index}`, defaultValue: 0.0 },
                { name: `a2_${index}`, defaultValue: 0.0 }
            ]).flat()
        ];
    }

    constructor() {
        super();
        this.sampleRate = sampleRate; // `sampleRate` is available globally here
        console.log('Sample rate:', this.sampleRate);
        // Set up state variables for the 8 biquad filters, initialize to 0
        this.biquadState = Array.from({ length: 8 }, () => ({ state1: 0.0, state2: 0.0 }));
        this.volumeEnvelope = [];
        this.wavetablemode = false;
        this.envelopeIndex = 0;
        this.currentSample = 0;
        this.port.onmessage = (event) => {
            if (event.data.type === 'volumeEnvelope') {
                this.volumeEnvelope = event.data.envelope;
                this.dx = event.data.dx;
                this.envelopeIndex = 0;
                this.currentSample = 0;
                this.wavetablemode = event.data.wavetablemode;

            }
        };
    }

    process(inputs, outputs, parameters) {
        const input = inputs[0];
        const output = outputs[0];
        const gainParam = parameters.gain;
        const pregain = gainParam[0];
        for (let channel = 0; channel < input.length; channel++) {
            const inputChannel = input[channel];
            const outputChannel = output[channel];
            let cs = this.currentSample;
            let dx = this.dx;
            for (let i = 0; i < inputChannel.length; i++, cs++) {
                let currentSample = inputChannel[i];

                // Run through 8 biquad filters in series
                if (1) for (let biquadIndex = 0; biquadIndex < 8; biquadIndex++) {
                    const b0 = parameters[`b0_${biquadIndex}`][0];
                    const b1 = parameters[`b1_${biquadIndex}`][0];
                    const b2 = parameters[`b2_${biquadIndex}`][0];
                    const a1 = parameters[`a1_${biquadIndex}`][0];
                    const a2 = parameters[`a2_${biquadIndex}`][0];
                    const state = this.biquadState[biquadIndex];
                    // Apply Direct Form II Transposed structure
                    const output = currentSample * b0 + state.state1;
                    state.state1 = currentSample * b1 - a1 * output + state.state2;
                    state.state2 = currentSample * b2 - a2 * output;
                    currentSample = output;
                }
                let postgain = 1.0;
                if (this.volumeEnvelope.length > 0) {
                    let x = cs * dx;
                    if (this.wavetablemode) x=x%1;
                    while (this.envelopeIndex < this.volumeEnvelope.length - 2 && x >= this.volumeEnvelope[this.envelopeIndex + 1].x)
                        this.envelopeIndex++;
                    while (this.envelopeIndex > 0 && x < this.volumeEnvelope[this.envelopeIndex].x)
                        this.envelopeIndex--;
                    const p1 = this.volumeEnvelope[this.envelopeIndex];
                    const p2 = this.volumeEnvelope[this.envelopeIndex + 1];
                    const t = Math.min(1, (x - p1.x) / (p2.x - p1.x));
                    postgain = p1.y + (p2.y - p1.y) * Math.pow(t, p1.power);
                    //if (cs < 16) postgain *= cs / 16;
                }
                outputChannel[i] = /*Math.tanh*/(currentSample * pregain) * postgain;
            }
        }
        if (input.length > 0)
            this.currentSample += input[0].length;
        return true;
    }
}

registerProcessor('custom-processor', CustomProcessor);
