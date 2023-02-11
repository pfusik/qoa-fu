// Generated automatically with "cito". Do not edit.

/**
 * Decoder of the "Quite OK Audio" format.
 */
public abstract class QOADecoder
{
	/**
	 * Constructs the decoder.
	 * The decoder can be used for several files, one after another.
	 */
	public QOADecoder()
	{
	}

	/**
	 * Reads a byte from the stream.
	 * Returns the unsigned byte value or -1 on EOF.
	 */
	protected abstract int readByte();

	/**
	 * Seeks the stream to the given position.
	 * @param position File offset in bytes.
	 */
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
	private int expectedFrameHeader;
	private int positionSamples;

	/**
	 * Reads the file header.
	 * Returns <code>true</code> if the header is valid.
	 */
	public final boolean readHeader()
	{
		if (readByte() != 'q' || readByte() != 'o' || readByte() != 'a' || readByte() != 'f')
			return false;
		this.bufferBits = this.buffer = 0;
		this.totalSamples = readBits(32);
		if (this.totalSamples <= 0)
			return false;
		this.expectedFrameHeader = readBits(32);
		if (this.expectedFrameHeader <= 0)
			return false;
		this.positionSamples = 0;
		int channels = getChannels();
		return channels > 0 && channels <= 8 && getSampleRate() > 0;
	}

	/**
	 * Returns the file length in samples per channel.
	 */
	public final int getTotalSamples()
	{
		return this.totalSamples;
	}

	/**
	 * Maximum number of channels supported by the format.
	 */
	public static final int MAX_CHANNELS = 8;

	/**
	 * Returns the number of audio channels.
	 */
	public final int getChannels()
	{
		return this.expectedFrameHeader >> 24;
	}

	/**
	 * Returns the sample rate in Hz.
	 */
	public final int getSampleRate()
	{
		return this.expectedFrameHeader & 16777215;
	}

	private static final int SLICE_SAMPLES = 20;

	private static final int MAX_FRAME_SLICES = 256;

	/**
	 * Maximum number of samples per frame.
	 */
	public static final int MAX_FRAME_SAMPLES = 5120;

	private int getMaxFrameBytes()
	{
		return 8 + getChannels() * 2064;
	}

	private static int clamp(int value, int min, int max)
	{
		return value < min ? min : value > max ? max : value;
	}

	private boolean readLMS(int[] result)
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

	/**
	 * Reads and decodes a frame.
	 * Returns the number of samples per channel.
	 * @param samples PCM samples.
	 */
	public final int readFrame(short[] samples)
	{
		if (this.positionSamples > 0 && readBits(32) != this.expectedFrameHeader)
			return -1;
		int samplesCount = readBits(16);
		if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > this.totalSamples - this.positionSamples)
			return -1;
		int channels = getChannels();
		int slices = (samplesCount + 19) / 20;
		if (readBits(16) != 8 + channels * (16 + slices * 8))
			return -1;
		final LMS[] lmses = new LMS[8];
		for (int _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS();
		}
		for (int c = 0; c < channels; c++) {
			if (!readLMS(lmses[c].history) || !readLMS(lmses[c].weights))
				return -1;
		}
		for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			for (int c = 0; c < channels; c++) {
				int scaleFactor = readBits(4);
				if (scaleFactor < 0)
					return -1;
				scaleFactor = READ_FRAME_SCALE_FACTORS[scaleFactor];
				int sampleOffset = sampleIndex * channels + c;
				for (int s = 0; s < 20; s++) {
					int quantized = readBits(3);
					if (quantized < 0)
						return -1;
					if (sampleIndex + s >= samplesCount)
						continue;
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
					if ((quantized & 1) != 0)
						dequantized = -dequantized;
					int reconstructed = clamp(lmses[c].predict() + dequantized, -32768, 32767);
					lmses[c].update(reconstructed, dequantized);
					samples[sampleOffset] = (short) reconstructed;
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
	public final void seekToSample(int position)
	{
		int frame = position / 5120;
		seekToByte(frame == 0 ? 12 : 8 + frame * getMaxFrameBytes());
		this.positionSamples = frame * 5120;
	}

	/**
	 * Returns <code>true</code> if all frames have been read.
	 */
	public final boolean isEnd()
	{
		return this.positionSamples >= this.totalSamples;
	}

	private static final short[] READ_FRAME_SCALE_FACTORS = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };
}
