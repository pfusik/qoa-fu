// Generated automatically with "cito". Do not edit.
#include <algorithm>
#include "QOA.hpp"

void LMS::assign(const LMS * source)
{
	std::copy_n(source->history.data(), 4, this->history.data());
	std::copy_n(source->weights.data(), 4, this->weights.data());
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

int QOABase::clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
}

int QOABase::getChannels() const
{
	return this->frameHeader >> 24;
}

int QOABase::getSampleRate() const
{
	return this->frameHeader & 16777215;
}

int QOABase::getFrameBytes(int sampleCount) const
{
	int slices = (sampleCount + 19) / 20;
	return 8 + getChannels() * (16 + slices * 8);
}

int QOABase::dequantize(int quantized, int scaleFactor)
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

bool QOAEncoder::writeHeader(int totalSamples, int channels, int sampleRate)
{
	if (totalSamples <= 0 || channels <= 0 || channels > 8 || sampleRate <= 0 || sampleRate >= 16777216)
		return false;
	this->frameHeader = channels << 24 | sampleRate;
	for (int c = 0; c < channels; c++) {
		this->lMSes[c].history.fill(0);
		this->lMSes[c].weights[0] = 0;
		this->lMSes[c].weights[1] = 0;
		this->lMSes[c].weights[2] = -8192;
		this->lMSes[c].weights[3] = 16384;
	}
	int64_t magic = 1903124838;
	return writeLong(magic << 32 | totalSamples);
}

bool QOAEncoder::writeLMS(int const * a)
{
	int64_t a0 = a[0];
	int64_t a1 = a[1];
	int64_t a2 = a[2];
	return writeLong(a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | (a[3] & 65535));
}

bool QOAEncoder::writeFrame(int16_t const * samples, int samplesCount)
{
	if (samplesCount <= 0 || samplesCount > 5120)
		return false;
	int64_t header = this->frameHeader;
	if (!writeLong(header << 32 | samplesCount << 16 | getFrameBytes(samplesCount)))
		return false;
	int channels = getChannels();
	for (int c = 0; c < channels; c++) {
		if (!writeLMS(this->lMSes[c].history.data()) || !writeLMS(this->lMSes[c].weights.data()))
			return false;
	}
	LMS lms;
	LMS bestLMS;
	for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
		int sliceSamples = samplesCount - sampleIndex;
		if (sliceSamples > 20)
			sliceSamples = 20;
		for (int c = 0; c < channels; c++) {
			int64_t bestError = 9223372036854775807;
			int64_t bestSlice = 0;
			for (int scaleFactor = 0; scaleFactor < 16; scaleFactor++) {
				lms.assign(&this->lMSes[c]);
				static constexpr std::array<int, 16> reciprocals = { 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 };
				int reciprocal = reciprocals[scaleFactor];
				int64_t slice = scaleFactor;
				int64_t currentError = 0;
				for (int s = 0; s < sliceSamples; s++) {
					int sample = samples[(sampleIndex + s) * channels + c];
					int predicted = lms.predict();
					int residual = sample - predicted;
					int scaled = (residual * reciprocal + 32768) >> 16;
					if (scaled != 0)
						scaled += scaled < 0 ? 1 : -1;
					if (residual != 0)
						scaled += residual > 0 ? 1 : -1;
					static constexpr std::array<uint8_t, 17> quantTab = { 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
						6 };
					int quantized = quantTab[8 + clamp(scaled, -8, 8)];
					int dequantized = dequantize(quantized, scaleFactors[scaleFactor]);
					int reconstructed = clamp(predicted + dequantized, -32768, 32767);
					int error = sample - reconstructed;
					currentError += error * error;
					if (currentError >= bestError)
						break;
					lms.update(reconstructed, dequantized);
					slice = slice << 3 | quantized;
				}
				if (currentError < bestError) {
					bestError = currentError;
					bestSlice = slice;
					bestLMS.assign(&lms);
				}
			}
			this->lMSes[c].assign(&bestLMS);
			bestSlice <<= (20 - sliceSamples) * 3;
			if (!writeLong(bestSlice))
				return false;
		}
	}
	return true;
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
	this->frameHeader = readBits(32);
	if (this->frameHeader <= 0)
		return false;
	this->positionSamples = 0;
	int channels = getChannels();
	return channels > 0 && channels <= 8 && getSampleRate() > 0;
}

int QOADecoder::getTotalSamples() const
{
	return this->totalSamples;
}

int QOADecoder::getMaxFrameBytes() const
{
	return 8 + getChannels() * 2064;
}

bool QOADecoder::readLMS(int * result)
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

int QOADecoder::readFrame(int16_t * samples)
{
	if (this->positionSamples > 0 && readBits(32) != this->frameHeader)
		return -1;
	int samplesCount = readBits(16);
	if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > this->totalSamples - this->positionSamples)
		return -1;
	int channels = getChannels();
	int slices = (samplesCount + 19) / 20;
	if (readBits(16) != 8 + channels * (16 + slices * 8))
		return -1;
	std::array<LMS, 8> lmses;
	for (int c = 0; c < channels; c++) {
		if (!readLMS(lmses[c].history.data()) || !readLMS(lmses[c].weights.data()))
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
				samples[sampleOffset] = static_cast<int16_t>(reconstructed);
				sampleOffset += channels;
			}
		}
	}
	this->positionSamples += samplesCount;
	return samplesCount;
}

void QOADecoder::seekToSample(int position)
{
	int frame = position / 5120;
	seekToByte(frame == 0 ? 12 : 8 + frame * getMaxFrameBytes());
	this->positionSamples = frame * 5120;
}

bool QOADecoder::isEnd() const
{
	return this->positionSamples >= this->totalSamples;
}
