// Generated automatically with "cito". Do not edit.

/**
 * Least Mean Squares Filter.
 */
class LMS
{
	readonly history: Int32Array = new Int32Array(4);
	readonly weights: Int32Array = new Int32Array(4);

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
 * Decoder of the "Quite OK Audio" format.
 */
export abstract class QOADecoder
{
	/**
	 * Constructs the decoder.
	 * The decoder can be used for several files, one after another.
	 */
	public constructor()
	{
	}

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
	private buffer: number;
	private bufferBits: number;

	private readBits(bits: number): number
	{
		while (this.bufferBits < bits) {
			let b: number = this.readByte();
			if (b < 0)
				return -1;
			this.buffer = this.buffer << 8 | b;
			this.bufferBits += 8;
		}
		this.bufferBits -= bits;
		let result: number = this.buffer >> this.bufferBits;
		this.buffer &= (1 << this.bufferBits) - 1;
		return result;
	}
	private totalSamples: number;
	private expectedFrameHeader: number;
	private positionSamples: number;

	/**
	 * Reads the file header.
	 * Returns <code>true</code> if the header is valid.
	 */
	public readHeader(): boolean
	{
		if (this.readByte() != 113 || this.readByte() != 111 || this.readByte() != 97 || this.readByte() != 102)
			return false;
		this.bufferBits = this.buffer = 0;
		this.totalSamples = this.readBits(32);
		if (this.totalSamples <= 0)
			return false;
		this.expectedFrameHeader = this.readBits(32);
		if (this.expectedFrameHeader <= 0)
			return false;
		this.positionSamples = 0;
		let channels: number = this.getChannels();
		return channels > 0 && channels <= 8 && this.getSampleRate() > 0;
	}

	/**
	 * Returns the file length in samples per channel.
	 */
	public getTotalSamples(): number
	{
		return this.totalSamples;
	}

	/**
	 * Maximum number of channels supported by the format.
	 */
	public static readonly MAX_CHANNELS: number = 8;

	/**
	 * Returns the number of audio channels.
	 */
	public getChannels(): number
	{
		return this.expectedFrameHeader >> 24;
	}

	/**
	 * Returns the sample rate in Hz.
	 */
	public getSampleRate(): number
	{
		return this.expectedFrameHeader & 16777215;
	}

	private static readonly SLICE_SAMPLES: number = 20;

	private static readonly MAX_FRAME_SLICES: number = 256;

	/**
	 * Maximum number of samples per frame.
	 */
	public static readonly MAX_FRAME_SAMPLES: number = 5120;

	private getMaxFrameBytes(): number
	{
		return 8 + this.getChannels() * 2064;
	}

	private static clamp(value: number, min: number, max: number): number
	{
		return value < min ? min : value > max ? max : value;
	}

	private readLMS(result: Int32Array | null): boolean
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
	public readFrame(samples: Int16Array | null): number
	{
		if (this.positionSamples > 0 && this.readBits(32) != this.expectedFrameHeader)
			return -1;
		let samplesCount: number = this.readBits(16);
		if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > this.totalSamples - this.positionSamples)
			return -1;
		let channels: number = this.getChannels();
		let slices: number = (samplesCount + 19) / 20 | 0;
		if (this.readBits(16) != 8 + channels * (16 + slices * 8))
			return -1;
		const lmses: LMS[] = new Array(8);
		for (let _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS();
		}
		for (let c: number = 0; c < channels; c++) {
			if (!this.readLMS(lmses[c].history) || !this.readLMS(lmses[c].weights))
				return -1;
		}
		for (let sampleIndex: number = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			for (let c: number = 0; c < channels; c++) {
				let scaleFactor: number = this.readBits(4);
				if (scaleFactor < 0)
					return -1;
				scaleFactor = QOADecoder.READ_FRAME_SCALE_FACTORS[scaleFactor];
				let sampleOffset: number = sampleIndex * channels + c;
				for (let s: number = 0; s < 20; s++) {
					let quantized: number = this.readBits(3);
					if (quantized < 0)
						return -1;
					if (sampleIndex + s >= samplesCount)
						continue;
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
					if ((quantized & 1) != 0)
						dequantized = -dequantized;
					let reconstructed: number = QOADecoder.clamp(lmses[c].predict() + dequantized, -32768, 32767);
					lmses[c].update(reconstructed, dequantized);
					samples[sampleOffset] = reconstructed;
					sampleOffset += channels;
				}
			}
		}
		this.positionSamples += samplesCount;
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
		this.seekToByte(frame == 0 ? 12 : 8 + frame * this.getMaxFrameBytes());
		this.positionSamples = frame * 5120;
	}

	/**
	 * Returns <code>true</code> if all frames have been read.
	 */
	public isEnd(): boolean
	{
		return this.positionSamples >= this.totalSamples;
	}

	private static readonly READ_FRAME_SCALE_FACTORS: Readonly<Uint16Array> = new Uint16Array([ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ]);
}
