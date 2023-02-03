// Generated automatically with "cito". Do not edit.

/// <summary>Least Mean Squares Filter.</summary>
class LMS
{

	readonly int[] History = new int[4];

	readonly int[] Weights = new int[4];

	internal void Init(int i, int h, int w)
	{
		this.History[i] = ((h ^ 128) - 128) << 8;
		this.Weights[i] = ((w ^ 128) - 128) << 8;
	}

	internal int Predict() => (this.History[0] * this.Weights[0] + this.History[1] * this.Weights[1] + this.History[2] * this.Weights[2] + this.History[3] * this.Weights[3]) >> 13;

	internal void Update(int sample, int residual)
	{
		int delta = residual >> 4;
		this.Weights[0] += this.History[0] < 0 ? -delta : delta;
		this.Weights[1] += this.History[1] < 0 ? -delta : delta;
		this.Weights[2] += this.History[2] < 0 ? -delta : delta;
		this.Weights[3] += this.History[3] < 0 ? -delta : delta;
		this.History[0] = this.History[1];
		this.History[1] = this.History[2];
		this.History[2] = this.History[3];
		this.History[3] = sample;
	}
}

/// <summary>Decoder of the "Quite OK Audio" format.</summary>
public abstract class QOADecoder
{
	/// <summary>Constructs the decoder.</summary>
	/// <remarks>The decoder can be used for several files, one after another.</remarks>
	public QOADecoder()
	{
	}

	/// <summary>Reads a byte from the stream.</summary>
	/// <remarks>Returns the unsigned byte value or -1 on EOF.</remarks>
	protected abstract int ReadByte();

	int Buffer;

	int BufferBits;

	int ReadBits(int bits)
	{
		while (this.BufferBits < bits) {
			int b = ReadByte();
			if (b < 0)
				return -1;
			this.Buffer = this.Buffer << 8 | b;
			this.BufferBits += 8;
		}
		this.BufferBits -= bits;
		int result = this.Buffer >> this.BufferBits;
		this.Buffer &= (1 << this.BufferBits) - 1;
		return result;
	}

	int TotalSamples;

	int ExpectedFrameHeader;

	int PositionSamples;

	/// <summary>Reads the file header.</summary>
	/// <remarks>Returns <see langword="true" /> if the header is valid.</remarks>
	public bool ReadHeader()
	{
		if (ReadByte() != 'q' || ReadByte() != 'o' || ReadByte() != 'a' || ReadByte() != 'f')
			return false;
		this.BufferBits = this.Buffer = 0;
		this.TotalSamples = ReadBits(32);
		if (this.TotalSamples <= 0)
			return false;
		this.ExpectedFrameHeader = ReadBits(32);
		if (this.ExpectedFrameHeader <= 0)
			return false;
		this.PositionSamples = 0;
		int channels = GetChannels();
		return channels > 0 && channels <= 8 && GetSampleRate() > 0;
	}

	/// <summary>Returns the file length in samples per channel.</summary>
	public int GetTotalSamples() => this.TotalSamples;

	/// <summary>Maximum number of channels supported by the format.</summary>
	public const int MaxChannels = 8;

	/// <summary>Returns the number of audio channels.</summary>
	public int GetChannels() => this.ExpectedFrameHeader >> 24;

	/// <summary>Returns the sample rate in Hz.</summary>
	public int GetSampleRate() => this.ExpectedFrameHeader & 16777215;

	const int SliceSamples = 20;

	const int FrameSlices = 256;

	/// <summary>Number of samples per frame.</summary>
	public const int FrameSamples = 5120;

	int GetFrameBytes() => 8 + GetChannels() * 2056;

	static int Clamp(int value, int min, int max) => value < min ? min : value > max ? max : value;

	/// <summary>Reads and decodes a frame.</summary>
	/// <remarks>Returns the number of samples per channel.</remarks>
	/// <param name="output">PCM samples.</param>
	public int ReadFrame(short[] output)
	{
		if (this.PositionSamples > 0 && ReadBits(32) != this.ExpectedFrameHeader)
			return -1;
		int samples = ReadBits(16);
		if (samples <= 0 || samples > 5120 || samples > this.TotalSamples - this.PositionSamples)
			return -1;
		int channels = GetChannels();
		int slices = (samples + 19) / 20;
		if (ReadBits(16) != 8 + channels * (8 + slices * 8))
			return -1;
		LMS[] lmses = new LMS[8];
		for (int _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS();
		}
		for (int c = 0; c < channels; c++) {
			for (int i = 0; i < 4; i++) {
				int h = ReadByte();
				if (h < 0)
					return -1;
				int w = ReadByte();
				if (w < 0)
					return -1;
				lmses[c].Init(i, h, w);
			}
		}
		for (int sampleIndex = 0; sampleIndex < samples; sampleIndex += 20) {
			for (int c = 0; c < channels; c++) {
				int scaleFactor = ReadBits(4);
				scaleFactor = ReadFramescaleFactors[scaleFactor];
				int sampleOffset = sampleIndex * channels + c;
				for (int s = 0; s < 20; s++) {
					int quantized = ReadBits(3);
					if (quantized < 0)
						return -1;
					if (sampleIndex + s >= samples)
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
					int reconstructed = Clamp(lmses[c].Predict() + dequantized, -32768, 32767);
					lmses[c].Update(reconstructed, dequantized);
					output[sampleOffset] = (short) reconstructed;
					sampleOffset += channels;
				}
			}
		}
		this.PositionSamples += samples;
		return samples;
	}

	/// <summary>Returns <see langword="true" /> if all frames have been read.</summary>
	public bool IsEnd() => this.PositionSamples >= this.TotalSamples;

	static readonly ushort[] ReadFramescaleFactors = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };
}
