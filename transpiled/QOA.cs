// Generated automatically with "cito". Do not edit.

/// <summary>Least Mean Squares Filter.</summary>
class LMS
{

	internal readonly int[] History = new int[4];

	internal readonly int[] Weights = new int[4];

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

	/// <summary>Seeks the stream to the given position.</summary>
	/// <param name="position">File offset in bytes.</param>
	protected abstract void SeekToByte(int position);

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

	const int MaxFrameSlices = 256;

	/// <summary>Maximum number of samples per frame.</summary>
	public const int MaxFrameSamples = 5120;

	int GetMaxFrameBytes() => 8 + GetChannels() * 2064;

	static int Clamp(int value, int min, int max) => value < min ? min : value > max ? max : value;

	bool ReadLMS(int[] result)
	{
		for (int i = 0; i < 4; i++) {
			int hi = ReadByte();
			if (hi < 0)
				return false;
			int lo = ReadByte();
			if (lo < 0)
				return false;
			result[i] = ((hi ^ 128) - 128) << 8 | lo;
		}
		return true;
	}

	/// <summary>Reads and decodes a frame.</summary>
	/// <remarks>Returns the number of samples per channel.</remarks>
	/// <param name="samples">PCM samples.</param>
	public int ReadFrame(short[] samples)
	{
		if (this.PositionSamples > 0 && ReadBits(32) != this.ExpectedFrameHeader)
			return -1;
		int samplesCount = ReadBits(16);
		if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > this.TotalSamples - this.PositionSamples)
			return -1;
		int channels = GetChannels();
		int slices = (samplesCount + 19) / 20;
		if (ReadBits(16) != 8 + channels * (16 + slices * 8))
			return -1;
		LMS[] lmses = new LMS[8];
		for (int _i0 = 0; _i0 < 8; _i0++) {
			lmses[_i0] = new LMS();
		}
		for (int c = 0; c < channels; c++) {
			if (!ReadLMS(lmses[c].History) || !ReadLMS(lmses[c].Weights))
				return -1;
		}
		for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
			for (int c = 0; c < channels; c++) {
				int scaleFactor = ReadBits(4);
				if (scaleFactor < 0)
					return -1;
				scaleFactor = ReadFramescaleFactors[scaleFactor];
				int sampleOffset = sampleIndex * channels + c;
				for (int s = 0; s < 20; s++) {
					int quantized = ReadBits(3);
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
					int reconstructed = Clamp(lmses[c].Predict() + dequantized, -32768, 32767);
					lmses[c].Update(reconstructed, dequantized);
					samples[sampleOffset] = (short) reconstructed;
					sampleOffset += channels;
				}
			}
		}
		this.PositionSamples += samplesCount;
		return samplesCount;
	}

	/// <summary>Seeks to the given time offset.</summary>
	/// <remarks>Requires the input stream to be seekable with <c>SeekToByte</c>.</remarks>
	/// <param name="position">Position from the beginning of the file.</param>
	public void SeekToSample(int position)
	{
		int frame = position / 5120;
		SeekToByte(frame == 0 ? 12 : 8 + frame * GetMaxFrameBytes());
		this.PositionSamples = frame * 5120;
	}

	/// <summary>Returns <see langword="true" /> if all frames have been read.</summary>
	public bool IsEnd() => this.PositionSamples >= this.TotalSamples;

	static readonly ushort[] ReadFramescaleFactors = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };
}
