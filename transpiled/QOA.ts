// Generated automatically with "fut". Do not edit.

/**
 * Least Mean Squares Filter.
 */
class LMS
{
	readonly history: Int32Array = new Int32Array(4);
	readonly weights: Int32Array = new Int32Array(4);

	assign(source: LMS): void
	{
		this.history.set(source.history);
		this.weights.set(source.weights);
	}

	predict(): number
	{
		return (this.history[0] * this.weights[0] + this.history[1] * this.weights[1] + this.history[2] * this.weights[2] + this.history[3] * this.weights[3]) >> 13;
	}

	update(sample: number, residual: number): void
	{
		let delta: number = residual >> 4;
		this.weights[0] += this.history[0] < 0 ? -delta : delta;
		this.weights[1] += this.history[1] < 0 ? -delta : delta;
		this.weights[2] += this.history[2] < 0 ? -delta : delta;
		this.weights[3] += this.history[3] < 0 ? -delta : delta;
		this.history[0] = this.history[1];
		this.history[1] = this.history[2];
		this.history[2] = this.history[3];
		this.history[3] = sample;
	}
}

/**
 * Common part of the "Quite OK Audio" format encoder and decoder.
 */
export abstract class QOABase
{

	protected static clamp(value: number, min: number, max: number): number
	{
		return value < min ? min : value > max ? max : value;
	}
	protected frameHeader: number;

	/**
	 * Maximum number of channels supported by the format.
	 */
	public static readonly MAX_CHANNELS: number = 8;

	/**
	 * Returns the number of audio channels.
	 */
	public getChannels(): number
	{
		return this.frameHeader >> 24;
	}

	/**
	 * Returns the sample rate in Hz.
	 */
	public getSampleRate(): number
	{
		return this.frameHeader & 16777215;
	}

	protected static readonly SLICE_SAMPLES: number = 20;

	protected static readonly MAX_FRAME_SLICES: number = 256;

	/**
	 * Maximum number of samples per frame.
	 */
	public static readonly MAX_FRAME_SAMPLES: number = 5120;

	protected getFrameBytes(sampleCount: number): number
	{
		let slices: number = (sampleCount + 19) / 20 | 0;
		return 8 + this.getChannels() * (16 + slices * 8);
	}

	protected static readonly SCALE_FACTORS: Readonly<Int16Array> = new Int16Array([ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ]);

	protected static dequantize(quantized: number, scaleFactor: number): number
	{
		let dequantized: number;
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
		return (quantized & 1) != 0 ? -dequantized : dequantized;
	}
}

/**
 * Encoder of the "Quite OK Audio" format.
 */
export abstract class QOAEncoder extends QOABase
{
	constructor()
	{
		super();
		for (let _i0 = 0; _i0 < 8; _i0++) {
			this.#lMSes[_i0] = new LMS();
		}
	}

	/**
	 * Writes the 64-bit integer in big endian order.
	 * Returns <code>true</code> on success.
	 * @param l The integer to be written to the QOA stream.
	 */
	protected abstract writeLong(l: bigint): boolean;
	readonly #lMSes: LMS[] = new Array(8);

	/**
	 * Writes the file header.
	 * Returns <code>true</code> on success.
	 * @param totalSamples File length in samples per channel.
	 * @param channels Number of audio channels.
	 * @param sampleRate Sample rate in Hz.
	 */
	public writeHeader(totalSamples: number, channels: number, sampleRate: number): boolean
	{
		if (totalSamples <= 0 || channels <= 0 || channels > 8 || sampleRate <= 0 || sampleRate >= 16777216)
			return false;
		this.frameHeader = channels << 24 | sampleRate;
		for (let c: number = 0; c < channels; c++) {
			this.#lMSes[c].history.fill(0);
			this.#lMSes[c].weights[0] = 0;
			this.#lMSes[c].weights[1] = 0;
			this.#lMSes[c].weights[2] = -8192;
			this.#lMSes[c].weights[3] = 16384;
		}
		let magic: bigint = 1903124838n;
		return this.writeLong(magic << 32n | BigInt(totalSamples));
	}

	#writeLMS(a: Readonly<Int32Array>): boolean
	{
		let a0: bigint = BigInt(a[0]);
		let a1: bigint = BigInt(a[1]);
		let a2: bigint = BigInt(a[2]);
		return this.writeLong(a0 << 48n | (a1 & 65535n) << 32n | (a2 & 65535n) << 16n | BigInt(a[3] & 65535));
	}

	/**
	 * Encodes and writes a frame.
	 * @param samples PCM samples: <code>samplesCount * channels</code> elements.
	 * @param samplesCount Number of samples per channel.
	 */
	public writeFrame(samples: Readonly<Int16Array>, samplesCount: number): boolean
	{
		if (samplesCount <= 0 || samplesCount > 5120)
			return false;
		let header: bigint = BigInt(this.frameHeader);
		if (!this.writeLong(header << 32n | BigInt(samplesCount << 16) | BigInt(this.getFrameBytes(samplesCount))))
			return false;
		let channels: number = this.getChannels();
		for (let c: number = 0; c < channels; c++) {
			if (!this.#writeLMS(this.#lMSes[c].history) || !this.#writeLMS(this.#lMSes[c].weights))
				return false;
		}
		const lms: LMS = new LMS();
		const bestLMS: LMS = new LMS();
		const lastScaleFactors: Uint8Array = new Uint8Array(8);
		for (let sampleIndex: number = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			let sliceSamples: number = samplesCount - sampleIndex;
			if (sliceSamples > 20)
				sliceSamples = 20;
			for (let c: number = 0; c < channels; c++) {
				let bestError: bigint = 9223372036854775807n;
				let bestSlice: bigint = 0n;
				for (let scaleFactorDelta: number = 0; scaleFactorDelta < 16; scaleFactorDelta++) {
					let scaleFactor: number = (lastScaleFactors[c] + scaleFactorDelta) & 15;
					lms.assign(this.#lMSes[c]);
					let reciprocal: number = QOAEncoder.#WRITE_FRAME_RECIPROCALS[scaleFactor];
					let slice: bigint = BigInt(scaleFactor);
					let currentError: bigint = 0n;
					for (let s: number = 0; s < sliceSamples; s++) {
						let sample: number = samples[(sampleIndex + s) * channels + c];
						let predicted: number = lms.predict();
						let residual: number = sample - predicted;
						let scaled: number = (residual * reciprocal + 32768) >> 16;
						if (scaled != 0)
							scaled += scaled < 0 ? 1 : -1;
						if (residual != 0)
							scaled += residual > 0 ? 1 : -1;
						let quantized: number = QOAEncoder.#WRITE_FRAME_QUANT_TAB[8 + QOAEncoder.clamp(scaled, -8, 8)];
						let dequantized: number = QOAEncoder.dequantize(quantized, QOAEncoder.SCALE_FACTORS[scaleFactor]);
						let reconstructed: number = QOAEncoder.clamp(predicted + dequantized, -32768, 32767);
						let error: bigint = BigInt(sample - reconstructed);
						currentError += error * error;
						if (currentError >= bestError)
							break;
						lms.update(reconstructed, dequantized);
						slice = slice << 3n | BigInt(quantized);
					}
					if (currentError < bestError) {
						bestError = currentError;
						bestSlice = slice;
						bestLMS.assign(lms);
					}
				}
				this.#lMSes[c].assign(bestLMS);
				bestSlice <<= (20 - sliceSamples) * 3;
				lastScaleFactors[c] = Number(bestSlice >> 60n);
				if (!this.writeLong(bestSlice))
					return false;
			}
		}
		return true;
	}

	static readonly #WRITE_FRAME_RECIPROCALS: Readonly<Int32Array> = new Int32Array([ 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 ]);

	static readonly #WRITE_FRAME_QUANT_TAB: Readonly<Uint8Array> = new Uint8Array([ 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
		6 ]);
}

/**
 * Decoder of the "Quite OK Audio" format.
 */
export abstract class QOADecoder extends QOABase
{

	/**
	 * Reads a byte from the stream.
	 * Returns the unsigned byte value or -1 on EOF.
	 */
	protected abstract readByte(): number;

	/**
	 * Seeks the stream to the given position.
	 * @param position File offset in bytes.
	 */
	protected abstract seekToByte(position: number): void;
	#buffer: number;
	#bufferBits: number;

	#readBits(bits: number): number
	{
		while (this.#bufferBits < bits) {
			let b: number = this.readByte();
			if (b < 0)
				return -1;
			this.#buffer = this.#buffer << 8 | b;
			this.#bufferBits += 8;
		}
		this.#bufferBits -= bits;
		let result: number = this.#buffer >> this.#bufferBits;
		this.#buffer &= (1 << this.#bufferBits) - 1;
		return result;
	}
	#totalSamples: number;
	#positionSamples: number;

	/**
	 * Reads the file header.
	 * Returns <code>true</code> if the header is valid.
	 */
	public readHeader(): boolean
	{
		if (this.readByte() != 113 || this.readByte() != 111 || this.readByte() != 97 || this.readByte() != 102)
			return false;
		this.#bufferBits = this.#buffer = 0;
		this.#totalSamples = this.#readBits(32);
		if (this.#totalSamples <= 0)
			return false;
		this.frameHeader = this.#readBits(32);
		if (this.frameHeader <= 0)
			return false;
		this.#positionSamples = 0;
		let channels: number = this.getChannels();
		return channels > 0 && channels <= 8 && this.getSampleRate() > 0;
	}

	/**
	 * Returns the file length in samples per channel.
	 */
	public getTotalSamples(): number
	{
		return this.#totalSamples;
	}

	#getMaxFrameBytes(): number
	{
		return 8 + this.getChannels() * 2064;
	}

	#readLMS(result: Int32Array): boolean
	{
		for (let i: number = 0; i < 4; i++) {
			let hi: number = this.readByte();
			if (hi < 0)
				return false;
			let lo: number = this.readByte();
			if (lo < 0)
				return false;
			result[i] = ((hi ^ 128) - 128) << 8 | lo;
		}
		return true;
	}

	/**
	 * Reads and decodes a frame.
	 * Returns the number of samples per channel.
	 * @param samples PCM samples.
	 */
	public readFrame(samples: Int16Array): number
	{
		if (this.#positionSamples > 0 && this.#readBits(32) != this.frameHeader)
			return -1;
		let samplesCount: number = this.#readBits(16);
		if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > this.#totalSamples - this.#positionSamples)
			return -1;
		let channels: number = this.getChannels();
		let slices: number = (samplesCount + 19) / 20 | 0;
		if (this.#readBits(16) != 8 + channels * (16 + slices * 8))
			return -1;
		const lmses: LMS[] = new Array(8);
		for (let _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS();
		}
		for (let c: number = 0; c < channels; c++) {
			if (!this.#readLMS(lmses[c].history) || !this.#readLMS(lmses[c].weights))
				return -1;
		}
		for (let sampleIndex: number = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			for (let c: number = 0; c < channels; c++) {
				let scaleFactor: number = this.#readBits(4);
				if (scaleFactor < 0)
					return -1;
				scaleFactor = QOADecoder.SCALE_FACTORS[scaleFactor];
				let sampleOffset: number = sampleIndex * channels + c;
				for (let s: number = 0; s < 20; s++) {
					let quantized: number = this.#readBits(3);
					if (quantized < 0)
						return -1;
					if (sampleIndex + s >= samplesCount)
						continue;
					let dequantized: number = QOADecoder.dequantize(quantized, scaleFactor);
					let reconstructed: number = QOADecoder.clamp(lmses[c].predict() + dequantized, -32768, 32767);
					lmses[c].update(reconstructed, dequantized);
					samples[sampleOffset] = reconstructed;
					sampleOffset += channels;
				}
			}
		}
		this.#positionSamples += samplesCount;
		return samplesCount;
	}

	/**
	 * Seeks to the given time offset.
	 * Requires the input stream to be seekable with <code>SeekToByte</code>.
	 * @param position Position from the beginning of the file.
	 */
	public seekToSample(position: number): void
	{
		let frame: number = position / 5120 | 0;
		this.seekToByte(frame == 0 ? 12 : 8 + frame * this.#getMaxFrameBytes());
		this.#positionSamples = frame * 5120;
	}

	/**
	 * Returns <code>true</code> if all frames have been read.
	 */
	public isEnd(): boolean
	{
		return this.#positionSamples >= this.#totalSamples;
	}
}
