// Generated automatically with "cito". Do not edit.

/**
 * Least Mean Squares Filter.
 */
class LMS
{
	#history = new Int32Array(4);
	#weights = new Int32Array(4);

	init(i, h, w)
	{
		this.#history[i] = ((h ^ 128) - 128) << 8;
		this.#weights[i] = ((w ^ 128) - 128) << 8;
	}

	predict()
	{
		return (this.#history[0] * this.#weights[0] + this.#history[1] * this.#weights[1] + this.#history[2] * this.#weights[2] + this.#history[3] * this.#weights[3]) >> 13;
	}

	update(sample, residual)
	{
		let delta = residual >> 4;
		this.#weights[0] += this.#history[0] < 0 ? -delta : delta;
		this.#weights[1] += this.#history[1] < 0 ? -delta : delta;
		this.#weights[2] += this.#history[2] < 0 ? -delta : delta;
		this.#weights[3] += this.#history[3] < 0 ? -delta : delta;
		this.#history[0] = this.#history[1];
		this.#history[1] = this.#history[2];
		this.#history[2] = this.#history[3];
		this.#history[3] = sample;
	}
}

/**
 * Decoder of the "Quite OK Audio" format.
 */
export class QOADecoder
{
	/**
	 * Constructs the decoder.
	 * The decoder can be used for several files, one after another.
	 */
	constructor()
	{
	}
	#buffer;
	#bufferBits;

	#readBits(bits)
	{
		while (this.#bufferBits < bits) {
			let b = this.readByte();
			if (b < 0)
				return -1;
			this.#buffer = this.#buffer << 8 | b;
			this.#bufferBits += 8;
		}
		this.#bufferBits -= bits;
		let result = this.#buffer >> this.#bufferBits;
		this.#buffer &= (1 << this.#bufferBits) - 1;
		return result;
	}
	#totalSamples;
	#expectedFrameHeader;
	#positionSamples;

	/**
	 * Reads the file header.
	 * Returns <code>true</code> if the header is valid.
	 */
	readHeader()
	{
		if (this.readByte() != 113 || this.readByte() != 111 || this.readByte() != 97 || this.readByte() != 102)
			return false;
		this.#bufferBits = this.#buffer = 0;
		this.#totalSamples = this.#readBits(32);
		if (this.#totalSamples <= 0)
			return false;
		this.#expectedFrameHeader = this.#readBits(32);
		if (this.#expectedFrameHeader <= 0)
			return false;
		this.#positionSamples = 0;
		let channels = this.getChannels();
		return channels > 0 && channels <= 8 && this.getSampleRate() > 0;
	}

	/**
	 * Returns the file length in samples per channel.
	 */
	getTotalSamples()
	{
		return this.#totalSamples;
	}

	/**
	 * Maximum number of channels supported by the format.
	 */
	static MAX_CHANNELS = 8;

	/**
	 * Returns the number of audio channels.
	 */
	getChannels()
	{
		return this.#expectedFrameHeader >> 24;
	}

	/**
	 * Returns the sample rate in Hz.
	 */
	getSampleRate()
	{
		return this.#expectedFrameHeader & 16777215;
	}

	/**
	 * Number of samples per frame.
	 */
	static FRAME_SAMPLES = 5120;

	#getFrameBytes()
	{
		return 8 + this.getChannels() * 2056;
	}

	static #clamp(value, min, max)
	{
		return value < min ? min : value > max ? max : value;
	}

	/**
	 * Reads and decodes a frame.
	 * Returns the number of samples per channel.
	 * @param output PCM samples.
	 */
	readFrame(output)
	{
		if (this.#positionSamples > 0 && this.#readBits(32) != this.#expectedFrameHeader)
			return -1;
		let samples = this.#readBits(16);
		if (samples <= 0 || samples > 5120 || samples > this.#totalSamples - this.#positionSamples)
			return -1;
		let channels = this.getChannels();
		let slices = (samples + 19) / 20 | 0;
		if (this.#readBits(16) != 8 + channels * (8 + slices * 8))
			return -1;
		const lmses = new Array(8);
		for (let _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS();
		}
		for (let c = 0; c < channels; c++) {
			for (let i = 0; i < 4; i++) {
				let h = this.readByte();
				if (h < 0)
					return -1;
				let w = this.readByte();
				if (w < 0)
					return -1;
				lmses[c].init(i, h, w);
			}
		}
		for (let sampleIndex = 0; sampleIndex < samples; sampleIndex += 20) {
			for (let c = 0; c < channels; c++) {
				let scaleFactor = this.#readBits(4);
				if (scaleFactor < 0)
					return -1;
				scaleFactor = QOADecoder.READ_FRAME_SCALE_FACTORS[scaleFactor];
				let sampleOffset = sampleIndex * channels + c;
				for (let s = 0; s < 20; s++) {
					let quantized = this.#readBits(3);
					if (quantized < 0)
						return -1;
					if (sampleIndex + s >= samples)
						continue;
					let dequantized;
					switch (quantized >> 1) {
					case 0:
						dequantized = (scaleFactor * 3 + 2) >> 2;
						break;
					case 1:
						dequantized = (scaleFactor * 5 + 1) >> 1;
						break;
					case 2:
						dequantized = (scaleFactor * 9 + 1) >> 1;
						break;
					default:
						dequantized = scaleFactor * 7;
						break;
					}
					if ((quantized & 1) != 0)
						dequantized = -dequantized;
					let reconstructed = QOADecoder.#clamp(lmses[c].predict() + dequantized, -32768, 32767);
					lmses[c].update(reconstructed, dequantized);
					output[sampleOffset] = reconstructed;
					sampleOffset += channels;
				}
			}
		}
		this.#positionSamples += samples;
		return samples;
	}

	/**
	 * Seeks to the given time offset.
	 * Requires the input stream to be seekable with <code>SeekToByte</code>.
	 * @param position Position from the beginning of the file.
	 */
	seekToSample(position)
	{
		let frame = position / 5120 | 0;
		this.seekToByte(frame == 0 ? 12 : 8 + frame * this.#getFrameBytes());
		this.#positionSamples = frame * 5120;
	}

	/**
	 * Returns <code>true</code> if all frames have been read.
	 */
	isEnd()
	{
		return this.#positionSamples >= this.#totalSamples;
	}

	static READ_FRAME_SCALE_FACTORS = new Uint16Array([ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ]);
}
