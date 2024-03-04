// Generated automatically with "fut". Do not edit.
import java.util.Arrays;

/**
 * Encoder of the "Quite OK Audio" format.
 */
public abstract class QOAEncoder extends QOABase
{
	QOAEncoder()
	{
		for (int _i0 = 0; _i0 < 8; _i0++) {
			this.lMSes[_i0] = new LMS();
		}
	}

	/**
	 * Writes the 64-bit integer in big endian order.
	 * Returns <code>true</code> on success.
	 * @param l The integer to be written to the QOA stream.
	 */
	protected abstract boolean writeLong(long l);
	private final LMS[] lMSes = new LMS[8];

	/**
	 * Writes the file header.
	 * Returns <code>true</code> on success.
	 * @param totalSamples File length in samples per channel.
	 * @param channels Number of audio channels.
	 * @param sampleRate Sample rate in Hz.
	 */
	public final boolean writeHeader(int totalSamples, int channels, int sampleRate)
	{
		if (totalSamples <= 0 || channels <= 0 || channels > 8 || sampleRate <= 0 || sampleRate >= 16777216)
			return false;
		this.frameHeader = channels << 24 | sampleRate;
		for (int c = 0; c < channels; c++) {
			Arrays.fill(this.lMSes[c].history, 0);
			this.lMSes[c].weights[0] = 0;
			this.lMSes[c].weights[1] = 0;
			this.lMSes[c].weights[2] = -8192;
			this.lMSes[c].weights[3] = 16384;
		}
		long magic = 1903124838;
		return writeLong(magic << 32 | totalSamples);
	}

	private boolean writeLMS(int[] a)
	{
		long a0 = a[0];
		long a1 = a[1];
		long a2 = a[2];
		return writeLong(a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | (a[3] & 65535));
	}

	/**
	 * Encodes and writes a frame.
	 * @param samples PCM samples: <code>samplesCount * channels</code> elements.
	 * @param samplesCount Number of samples per channel.
	 */
	public final boolean writeFrame(short[] samples, int samplesCount)
	{
		if (samplesCount <= 0 || samplesCount > 5120)
			return false;
		long header = this.frameHeader;
		if (!writeLong(header << 32 | samplesCount << 16 | getFrameBytes(samplesCount)))
			return false;
		int channels = getChannels();
		for (int c = 0; c < channels; c++) {
			if (!writeLMS(this.lMSes[c].history) || !writeLMS(this.lMSes[c].weights))
				return false;
		}
		final LMS lms = new LMS();
		final LMS bestLMS = new LMS();
		final byte[] lastScaleFactors = new byte[8];
		for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			int sliceSamples = samplesCount - sampleIndex;
			if (sliceSamples > 20)
				sliceSamples = 20;
			for (int c = 0; c < channels; c++) {
				long bestRank = 9223372036854775807L;
				long bestSlice = 0;
				for (int scaleFactorDelta = 0; scaleFactorDelta < 16; scaleFactorDelta++) {
					int scaleFactor = (lastScaleFactors[c] + scaleFactorDelta) & 15;
					lms.assign(this.lMSes[c]);
					int reciprocal = WRITE_FRAME_RECIPROCALS[scaleFactor];
					long slice = scaleFactor;
					long currentRank = 0;
					for (int s = 0; s < sliceSamples; s++) {
						int sample = samples[(sampleIndex + s) * channels + c];
						int predicted = lms.predict();
						int residual = sample - predicted;
						int scaled = (residual * reciprocal + 32768) >> 16;
						if (scaled != 0)
							scaled += scaled < 0 ? 1 : -1;
						if (residual != 0)
							scaled += residual > 0 ? 1 : -1;
						int quantized = WRITE_FRAME_QUANT_TAB[8 + clamp(scaled, -8, 8)];
						int dequantized = dequantize(quantized, SCALE_FACTORS[scaleFactor]);
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
				lastScaleFactors[c] = (byte) (bestSlice >> 60);
				if (!writeLong(bestSlice))
					return false;
			}
		}
		return true;
	}

	private static final int[] WRITE_FRAME_RECIPROCALS = { 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 };

	private static final byte[] WRITE_FRAME_QUANT_TAB = { 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
		6 };
}
