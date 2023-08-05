// Generated automatically with "fut". Do not edit.
#pragma once
#include <array>
#include <cstdint>
class LMS;
class QOABase;
class QOAEncoder;
class QOADecoder;

/**
 * Least Mean Squares Filter.
 */
class LMS
{
public:
	LMS() = default;
public:
	std::array<int, 4> history;
	std::array<int, 4> weights;
	void assign(const LMS * source);
	int predict() const;
	void update(int sample, int residual);
};

/**
 * Common part of the "Quite OK Audio" format encoder and decoder.
 */
class QOABase
{
public:
	virtual ~QOABase() = default;
	/**
	 * Maximum number of channels supported by the format.
	 */
	static constexpr int maxChannels = 8;
	/**
	 * Returns the number of audio channels.
	 */
	int getChannels() const;
	/**
	 * Returns the sample rate in Hz.
	 */
	int getSampleRate() const;
	/**
	 * Maximum number of samples per frame.
	 */
	static constexpr int maxFrameSamples = 5120;
protected:
	QOABase() = default;
	static int clamp(int value, int min, int max);
	int frameHeader;
	static constexpr int sliceSamples = 20;
	static constexpr int maxFrameSlices = 256;
	int getFrameBytes(int sampleCount) const;
	static constexpr std::array<int16_t, 16> scaleFactors = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };
	static int dequantize(int quantized, int scaleFactor);
};

/**
 * Encoder of the "Quite OK Audio" format.
 */
class QOAEncoder : public QOABase
{
public:
	virtual ~QOAEncoder() = default;
	/**
	 * Writes the file header.
	 * Returns <code>true</code> on success.
	 * @param totalSamples File length in samples per channel.
	 * @param channels Number of audio channels.
	 * @param sampleRate Sample rate in Hz.
	 */
	bool writeHeader(int totalSamples, int channels, int sampleRate);
	/**
	 * Encodes and writes a frame.
	 * @param samples PCM samples: <code>samplesCount * channels</code> elements.
	 * @param samplesCount Number of samples per channel.
	 */
	bool writeFrame(int16_t const * samples, int samplesCount);
protected:
	QOAEncoder() = default;
	/**
	 * Writes the 64-bit integer in big endian order.
	 * Returns <code>true</code> on success.
	 * @param l The integer to be written to the QOA stream.
	 */
	virtual bool writeLong(int64_t l) = 0;
private:
	std::array<LMS, 8> lMSes;
	bool writeLMS(int const * a);
};

/**
 * Decoder of the "Quite OK Audio" format.
 */
class QOADecoder : public QOABase
{
public:
	virtual ~QOADecoder() = default;
	/**
	 * Reads the file header.
	 * Returns <code>true</code> if the header is valid.
	 */
	bool readHeader();
	/**
	 * Returns the file length in samples per channel.
	 */
	int getTotalSamples() const;
	/**
	 * Reads and decodes a frame.
	 * Returns the number of samples per channel.
	 * @param samples PCM samples.
	 */
	int readFrame(int16_t * samples);
	/**
	 * Seeks to the given time offset.
	 * Requires the input stream to be seekable with <code>SeekToByte</code>.
	 * @param position Position from the beginning of the file.
	 */
	void seekToSample(int position);
	/**
	 * Returns <code>true</code> if all frames have been read.
	 */
	bool isEnd() const;
protected:
	QOADecoder() = default;
	/**
	 * Reads a byte from the stream.
	 * Returns the unsigned byte value or -1 on EOF.
	 */
	virtual int readByte() = 0;
	/**
	 * Seeks the stream to the given position.
	 * @param position File offset in bytes.
	 */
	virtual void seekToByte(int position) = 0;
private:
	int buffer;
	int bufferBits;
	int readBits(int bits);
	int totalSamples;
	int positionSamples;
	int getMaxFrameBytes() const;
	bool readLMS(int * result);
};
