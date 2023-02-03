// Generated automatically with "cito". Do not edit.
#include "QOA.hpp"

void LMS::init(int i, int h, int w)
{
	this->history[i] = ((h ^ 128) - 128) << 8;
	this->weights[i] = ((w ^ 128) - 128) << 8;
}

int LMS::predict() const
{
	return (this->history[0] * this->weights[0] + this->history[1] * this->weights[1] + this->history[2] * this->weights[2] + this->history[3] * this->weights[3]) >> 13;
}

void LMS::update(int sample, int residual)
{
	int delta = residual >> 4;
	this->weights[0] += this->history[0] < 0 ? -delta : delta;
	this->weights[1] += this->history[1] < 0 ? -delta : delta;
	this->weights[2] += this->history[2] < 0 ? -delta : delta;
	this->weights[3] += this->history[3] < 0 ? -delta : delta;
	this->history[0] = this->history[1];
	this->history[1] = this->history[2];
	this->history[2] = this->history[3];
	this->history[3] = sample;
}
QOADecoder::QOADecoder()
{
}

int QOADecoder::readBits(int bits)
{
	while (this->bufferBits < bits) {
		int b = readByte();
		if (b < 0)
			return -1;
		this->buffer = this->buffer << 8 | b;
		this->bufferBits += 8;
	}
	this->bufferBits -= bits;
	int result = this->buffer >> this->bufferBits;
	this->buffer &= (1 << this->bufferBits) - 1;
	return result;
}

bool QOADecoder::readHeader()
{
	if (readByte() != 'q' || readByte() != 'o' || readByte() != 'a' || readByte() != 'f')
		return false;
	this->bufferBits = this->buffer = 0;
	this->totalSamples = readBits(32);
	if (this->totalSamples <= 0)
		return false;
	this->expectedFrameHeader = readBits(32);
	if (this->expectedFrameHeader <= 0)
		return false;
	this->positionSamples = 0;
	int channels = getChannels();
	return channels > 0 && channels <= 8 && getSampleRate() > 0;
}

int QOADecoder::getTotalSamples() const
{
	return this->totalSamples;
}

int QOADecoder::getChannels() const
{
	return this->expectedFrameHeader >> 24;
}

int QOADecoder::getSampleRate() const
{
	return this->expectedFrameHeader & 16777215;
}

int QOADecoder::getFrameBytes() const
{
	return 8 + getChannels() * 2056;
}

int QOADecoder::clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
}

int QOADecoder::readFrame(int16_t * output)
{
	if (this->positionSamples > 0 && readBits(32) != this->expectedFrameHeader)
		return -1;
	int samples = readBits(16);
	if (samples <= 0 || samples > 5120 || samples > this->totalSamples - this->positionSamples)
		return -1;
	int channels = getChannels();
	int slices = (samples + 19) / 20;
	if (readBits(16) != 8 + channels * (8 + slices * 8))
		return -1;
	std::array<LMS, 8> lmses;
	for (int c = 0; c < channels; c++) {
		for (int i = 0; i < 4; i++) {
			int h = readByte();
			if (h < 0)
				return -1;
			int w = readByte();
			if (w < 0)
				return -1;
			lmses[c].init(i, h, w);
		}
	}
	for (int sampleIndex = 0; sampleIndex < samples; sampleIndex += 20) {
		for (int c = 0; c < channels; c++) {
			int scaleFactor = readBits(4);
			static constexpr std::array<uint16_t, 16> scaleFactors = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };
			scaleFactor = scaleFactors[scaleFactor];
			int sampleOffset = sampleIndex * channels + c;
			for (int s = 0; s < 20; s++) {
				int quantized = readBits(3);
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
				int reconstructed = clamp(lmses[c].predict() + dequantized, -32768, 32767);
				lmses[c].update(reconstructed, dequantized);
				output[sampleOffset] = static_cast<int16_t>(reconstructed);
				sampleOffset += channels;
			}
		}
	}
	this->positionSamples += samples;
	return samples;
}

bool QOADecoder::isEnd() const
{
	return this->positionSamples >= this->totalSamples;
}
