// Generated automatically with "fut". Do not edit.

/**
 * Common part of the "Quite OK Audio" format encoder and decoder.
 */
public abstract class QOABase
{
	protected int frameHeader;

	/**
	 * Maximum number of channels supported by the format.
	 */
	public static final int MAX_CHANNELS = 8;

	/**
	 * Returns the number of audio channels.
	 */
	public final int getChannels()
	{
		return this.frameHeader >> 24;
	}

	/**
	 * Returns the sample rate in Hz.
	 */
	public final int getSampleRate()
	{
		return this.frameHeader & 16777215;
	}

	protected static final int SLICE_SAMPLES = 20;

	protected static final int MAX_FRAME_SLICES = 256;

	/**
	 * Maximum number of samples per frame.
	 */
	public static final int MAX_FRAME_SAMPLES = 5120;

	protected final int getFrameBytes(int sampleCount)
	{
		int slices = (sampleCount + 19) / 20;
		return 8 + getChannels() * (16 + slices * 8);
	}

	protected static final short[] SCALE_FACTORS = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };

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
