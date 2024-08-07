// Generated automatically with "fut". Do not edit.
import std.algorithm;

/// Least Mean Squares Filter.
class LMS
{

	int[4] history;

	int[4] weights;

	void assign(LMS source)
	{
		source.history[0 .. 4].copy(this.history[]);
		source.weights[0 .. 4].copy(this.weights[]);
	}

	int predict() => (this.history[0] * this.weights[0] + this.history[1] * this.weights[1] + this.history[2] * this.weights[2] + this.history[3] * this.weights[3]) >> 13;

	void update(int sample, int residual)
	{
		int delta = residual >> 4;
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

/// Common part of the "Quite OK Audio" format encoder and decoder.
class QOABase
{

	protected int frameHeader;
	/// Maximum number of channels supported by the format.
	static immutable int maxChannels = 8;

	/// Returns the number of audio channels.
	int getChannels() => this.frameHeader >> 24;

	/// Returns the sample rate in Hz.
	int getSampleRate() => this.frameHeader & 16777215;
	static immutable int sliceSamples = 20;
	static immutable int maxFrameSlices = 256;
	/// Maximum number of samples per frame.
	static immutable int maxFrameSamples = 5120;

	protected int getFrameBytes(int sampleCount)
	{
		int slices = (sampleCount + 19) / 20;
		return 8 + getChannels() * (16 + slices * 8);
	}
	static immutable short[16] scaleFactors = [ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ];

	protected static int dequantize(int quantized, int scaleFactor)
	{
		int dequantized;
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

/// Encoder of the "Quite OK Audio" format.
class QOAEncoder : QOABase
{
	private this()
	{
		for (size_t _i0 = 0; _i0 < 8; _i0++) {
			this.lMSes[_i0] = new LMS;
		}
	}

	/// Writes the 64-bit integer in big endian order.
	/// Returns `true` on success.
	/// Params:
	/// l = The integer to be written to the QOA stream.
	protected abstract bool writeLong(long l);

	private LMS[8] lMSes;

	/// Writes the file header.
	/// Returns `true` on success.
	/// Params:
	/// totalSamples = File length in samples per channel.
	/// channels = Number of audio channels.
	/// sampleRate = Sample rate in Hz.
	bool writeHeader(int totalSamples, int channels, int sampleRate)
	{
		if (totalSamples <= 0 || channels <= 0 || channels > 8 || sampleRate <= 0 || sampleRate >= 16777216)
			return false;
		this.frameHeader = channels << 24 | sampleRate;
		for (int c = 0; c < channels; c++) {
			this.lMSes[c].history[].fill(0);
			this.lMSes[c].weights[0] = 0;
			this.lMSes[c].weights[1] = 0;
			this.lMSes[c].weights[2] = -8192;
			this.lMSes[c].weights[3] = 16384;
		}
		long magic = 1903124838;
		return writeLong(magic << 32 | totalSamples);
	}

	private bool writeLMS(const(int)[] a)
	{
		long a0 = a[0];
		long a1 = a[1];
		long a2 = a[2];
		return writeLong(a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | (a[3] & 65535));
	}

	/// Encodes and writes a frame.
	/// Params:
	/// samples = PCM samples: `samplesCount * channels` elements.
	/// samplesCount = Number of samples per channel.
	bool writeFrame(const(short)[] samples, int samplesCount)
	{
		if (samplesCount <= 0 || samplesCount > 5120)
			return false;
		long header = this.frameHeader;
		if (!writeLong(header << 32 | samplesCount << 16 | getFrameBytes(samplesCount)))
			return false;
		int channels = getChannels();
		for (int c = 0; c < channels; c++) {
			if (!writeLMS(this.lMSes[c].history[]) || !writeLMS(this.lMSes[c].weights[]))
				return false;
		}
		LMS lms = new LMS;
		LMS bestLMS = new LMS;
		ubyte[8] lastScaleFactors;
		for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			int sliceSamples = min(samplesCount - sampleIndex, 20);
			for (int c = 0; c < channels; c++) {
				long bestRank = 9223372036854775807;
				long bestSlice = 0;
				for (int scaleFactorDelta = 0; scaleFactorDelta < 16; scaleFactorDelta++) {
					int scaleFactor = (lastScaleFactors[c] + scaleFactorDelta) & 15;
					lms.assign(this.lMSes[c]);
					static immutable int[16] reciprocals = [ 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 ];
					int reciprocal = reciprocals[scaleFactor];
					long slice = scaleFactor;
					long currentRank = 0;
					for (int s = 0; s < sliceSamples; s++) {
						int sample = samples[(sampleIndex + s) * channels + c];
						int predicted = lms.predict();
						int residual = sample - predicted;
						int scaled = (residual * reciprocal + 32768) >> 16;
						if (scaled != 0)
							scaled += scaled < 0 ? cast(byte)(1) : cast(byte)(-1);
						if (residual != 0)
							scaled += residual > 0 ? cast(byte)(1) : cast(byte)(-1);
						static immutable ubyte[17] quantTab = [ 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
							6 ];
						int quantized = quantTab[8 + clamp(scaled, -8, 8)];
						int dequantized = dequantize(quantized, scaleFactors[scaleFactor]);
						int reconstructed = clamp(predicted + dequantized, -32768, 32767);
						long error = sample - reconstructed;
						currentRank += error * error;
						int weightsPenalty = ((lms.weights[0] * lms.weights[0] + lms.weights[1] * lms.weights[1] + lms.weights[2] * lms.weights[2] + lms.weights[3] * lms.weights[3]) >> 18) - 2303;
						if (weightsPenalty > 0)
							currentRank += weightsPenalty;
						if (currentRank >= bestRank)
							break;
						lms.update(reconstructed, dequantized);
						slice = slice << 3 | quantized;
					}
					if (currentRank < bestRank) {
						bestRank = currentRank;
						bestSlice = slice;
						bestLMS.assign(lms);
					}
				}
				this.lMSes[c].assign(bestLMS);
				bestSlice <<= (20 - sliceSamples) * 3;
				lastScaleFactors[c] = cast(ubyte)(bestSlice >> 60);
				if (!writeLong(bestSlice))
					return false;
			}
		}
		return true;
	}
}

/// Decoder of the "Quite OK Audio" format.
class QOADecoder : QOABase
{

	/// Reads a byte from the stream.
	/// Returns the unsigned byte value or -1 on EOF.
	protected abstract int readByte();

	/// Seeks the stream to the given position.
	/// Params:
	/// position = File offset in bytes.
	protected abstract void seekToByte(int position);

	private int buffer;

	private int bufferBits;

	private int readBits(int bits)
	{
		while (this.bufferBits < bits) {
			int b = readByte();
			if (b < 0)
				return -1;
			this.buffer = this.buffer << 8 | b;
			this.bufferBits += 8;
		}
		this.bufferBits -= bits;
		int result = this.buffer >> this.bufferBits;
		this.buffer &= (1 << this.bufferBits) - 1;
		return result;
	}

	private int totalSamples;

	private int positionSamples;

	/// Reads the file header.
	/// Returns `true` if the header is valid.
	bool readHeader()
	{
		if (readByte() != 'q' || readByte() != 'o' || readByte() != 'a' || readByte() != 'f')
			return false;
		this.bufferBits = this.buffer = 0;
		this.totalSamples = readBits(32);
		if (this.totalSamples <= 0)
			return false;
		this.frameHeader = readBits(32);
		if (this.frameHeader <= 0)
			return false;
		this.positionSamples = 0;
		int channels = getChannels();
		return channels > 0 && channels <= 8 && getSampleRate() > 0;
	}

	/// Returns the file length in samples per channel.
	int getTotalSamples() => this.totalSamples;

	private int getMaxFrameBytes() => 8 + getChannels() * 2064;

	private bool readLMS(int[] result)
	{
		for (int i = 0; i < 4; i++) {
			int hi = readByte();
			if (hi < 0)
				return false;
			int lo = readByte();
			if (lo < 0)
				return false;
			result[i] = ((hi ^ 128) - 128) << 8 | lo;
		}
		return true;
	}

	/// Reads and decodes a frame.
	/// Returns the number of samples per channel.
	/// Params:
	/// samples = PCM samples.
	int readFrame(short[] samples)
	{
		if (this.positionSamples > 0 && readBits(32) != this.frameHeader)
			return -1;
		int samplesCount = readBits(16);
		if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > this.totalSamples - this.positionSamples)
			return -1;
		int channels = getChannels();
		int slices = (samplesCount + 19) / 20;
		if (readBits(16) != 8 + channels * (16 + slices * 8))
			return -1;
		LMS[8] lmses;
		for (size_t _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS;
		}
		for (int c = 0; c < channels; c++) {
			if (!readLMS(lmses[c].history[]) || !readLMS(lmses[c].weights[]))
				return -1;
		}
		for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			for (int c = 0; c < channels; c++) {
				int scaleFactor = readBits(4);
				if (scaleFactor < 0)
					return -1;
				scaleFactor = scaleFactors[scaleFactor];
				int sampleOffset = sampleIndex * channels + c;
				for (int s = 0; s < 20; s++) {
					int quantized = readBits(3);
					if (quantized < 0)
						return -1;
					if (sampleIndex + s >= samplesCount)
						continue;
					int dequantized = dequantize(quantized, scaleFactor);
					int reconstructed = clamp(lmses[c].predict() + dequantized, -32768, 32767);
					lmses[c].update(reconstructed, dequantized);
					samples[sampleOffset] = cast(short)(reconstructed);
					sampleOffset += channels;
				}
			}
		}
		this.positionSamples += samplesCount;
		return samplesCount;
	}

	/// Seeks to the given time offset.
	/// Requires the input stream to be seekable with `SeekToByte`.
	/// Params:
	/// position = Position from the beginning of the file.
	void seekToSample(int position)
	{
		int frame = position / 5120;
		seekToByte(frame == 0 ? 12 : 8 + frame * getMaxFrameBytes());
		this.positionSamples = frame * 5120;
	}

	/// Returns `true` if all frames have been read.
	bool isEnd() => this.positionSamples >= this.totalSamples;
}
