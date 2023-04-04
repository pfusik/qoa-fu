// Generated automatically with "cito". Do not edit.
#include <stdlib.h>
#include <string.h>
#include "QOA.h"
typedef struct LMS LMS;

/**
 * Least Mean Squares Filter.
 */
struct LMS {
	int history[4];
	int weights[4];
};

static void LMS_Assign(LMS *self, const LMS *source);

static int LMS_Predict(const LMS *self);

static void LMS_Update(LMS *self, int sample, int residual);

/**
 * Common part of the "Quite OK Audio" format encoder and decoder.
 */
struct QOABase {
	int frameHeader;
};

static int QOABase_Clamp(int value, int min, int max);

static int QOABase_GetFrameBytes(const QOABase *self, int sampleCount);
static const int16_t QOABase_SCALE_FACTORS[16] = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };

static int QOABase_Dequantize(int quantized, int scaleFactor);

typedef struct {
	bool (*writeLong)(QOAEncoder *self, int64_t l);
} QOAEncoderVtbl;
/**
 * Encoder of the "Quite OK Audio" format.
 */
struct QOAEncoder {
	const QOAEncoderVtbl *vtbl;
	QOABase base;
	LMS lMSes[8];
};

static bool QOAEncoder_WriteLMS(QOAEncoder *self, int const *a);

typedef struct {
	int (*readByte)(QOADecoder *self);
	void (*seekToByte)(QOADecoder *self, int position);
} QOADecoderVtbl;
/**
 * Decoder of the "Quite OK Audio" format.
 */
struct QOADecoder {
	const QOADecoderVtbl *vtbl;
	QOABase base;
	int buffer;
	int bufferBits;
	int totalSamples;
	int positionSamples;
};

static int QOADecoder_ReadBits(QOADecoder *self, int bits);

static int QOADecoder_GetMaxFrameBytes(const QOADecoder *self);

static bool QOADecoder_ReadLMS(QOADecoder *self, int *result);

static void LMS_Assign(LMS *self, const LMS *source)
{
	memcpy(self->history, source->history, 4 * sizeof(int));
	memcpy(self->weights, source->weights, 4 * sizeof(int));
}

static int LMS_Predict(const LMS *self)
{
	return (self->history[0] * self->weights[0] + self->history[1] * self->weights[1] + self->history[2] * self->weights[2] + self->history[3] * self->weights[3]) >> 13;
}

static void LMS_Update(LMS *self, int sample, int residual)
{
	int delta = residual >> 4;
	self->weights[0] += self->history[0] < 0 ? -delta : delta;
	self->weights[1] += self->history[1] < 0 ? -delta : delta;
	self->weights[2] += self->history[2] < 0 ? -delta : delta;
	self->weights[3] += self->history[3] < 0 ? -delta : delta;
	self->history[0] = self->history[1];
	self->history[1] = self->history[2];
	self->history[2] = self->history[3];
	self->history[3] = sample;
}

static int QOABase_Clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
}

int QOABase_GetChannels(const QOABase *self)
{
	return self->frameHeader >> 24;
}

int QOABase_GetSampleRate(const QOABase *self)
{
	return self->frameHeader & 16777215;
}

static int QOABase_GetFrameBytes(const QOABase *self, int sampleCount)
{
	int slices = (sampleCount + 19) / 20;
	return 8 + QOABase_GetChannels(self) * (16 + slices * 8);
}

static int QOABase_Dequantize(int quantized, int scaleFactor)
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

bool QOAEncoder_WriteHeader(QOAEncoder *self, int totalSamples, int channels, int sampleRate)
{
	if (totalSamples <= 0 || channels <= 0 || channels > 8 || sampleRate <= 0 || sampleRate >= 16777216)
		return false;
	self->base.frameHeader = channels << 24 | sampleRate;
	for (int c = 0; c < channels; c++) {
		memset(self->lMSes[c].history, 0, sizeof(self->lMSes[c].history));
		self->lMSes[c].weights[0] = 0;
		self->lMSes[c].weights[1] = 0;
		self->lMSes[c].weights[2] = -8192;
		self->lMSes[c].weights[3] = 16384;
	}
	int64_t magic = 1903124838;
	return self->vtbl->writeLong(self, magic << 32 | totalSamples);
}

static bool QOAEncoder_WriteLMS(QOAEncoder *self, int const *a)
{
	int64_t a0 = a[0];
	int64_t a1 = a[1];
	int64_t a2 = a[2];
	return self->vtbl->writeLong(self, a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | (a[3] & 65535));
}

bool QOAEncoder_WriteFrame(QOAEncoder *self, int16_t const *samples, int samplesCount)
{
	if (samplesCount <= 0 || samplesCount > 5120)
		return false;
	int64_t header = self->base.frameHeader;
	if (!self->vtbl->writeLong(self, header << 32 | samplesCount << 16 | QOABase_GetFrameBytes(&self->base, samplesCount)))
		return false;
	int channels = QOABase_GetChannels(&self->base);
	for (int c = 0; c < channels; c++) {
		if (!QOAEncoder_WriteLMS(self, self->lMSes[c].history) || !QOAEncoder_WriteLMS(self, self->lMSes[c].weights))
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
				LMS_Assign(&lms, &self->lMSes[c]);
				static const int RECIPROCALS[16] = { 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 };
				int reciprocal = RECIPROCALS[scaleFactor];
				int64_t slice = scaleFactor;
				int64_t currentError = 0;
				for (int s = 0; s < sliceSamples; s++) {
					int sample = samples[(sampleIndex + s) * channels + c];
					int predicted = LMS_Predict(&lms);
					int residual = sample - predicted;
					int scaled = (residual * reciprocal + 32768) >> 16;
					if (scaled != 0)
						scaled += scaled < 0 ? 1 : -1;
					if (residual != 0)
						scaled += residual > 0 ? 1 : -1;
					static const uint8_t QUANT_TAB[17] = { 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
						6 };
					int quantized = QUANT_TAB[8 + QOABase_Clamp(scaled, -8, 8)];
					int dequantized = QOABase_Dequantize(quantized, QOABase_SCALE_FACTORS[scaleFactor]);
					int reconstructed = QOABase_Clamp(predicted + dequantized, -32768, 32767);
					int64_t error = sample - reconstructed;
					currentError += error * error;
					if (currentError >= bestError)
						break;
					LMS_Update(&lms, reconstructed, dequantized);
					slice = slice << 3 | quantized;
				}
				if (currentError < bestError) {
					bestError = currentError;
					bestSlice = slice;
					LMS_Assign(&bestLMS, &lms);
				}
			}
			LMS_Assign(&self->lMSes[c], &bestLMS);
			bestSlice <<= (20 - sliceSamples) * 3;
			if (!self->vtbl->writeLong(self, bestSlice))
				return false;
		}
	}
	return true;
}

static int QOADecoder_ReadBits(QOADecoder *self, int bits)
{
	while (self->bufferBits < bits) {
		int b = self->vtbl->readByte(self);
		if (b < 0)
			return -1;
		self->buffer = self->buffer << 8 | b;
		self->bufferBits += 8;
	}
	self->bufferBits -= bits;
	int result = self->buffer >> self->bufferBits;
	self->buffer &= (1 << self->bufferBits) - 1;
	return result;
}

bool QOADecoder_ReadHeader(QOADecoder *self)
{
	if (self->vtbl->readByte(self) != 'q' || self->vtbl->readByte(self) != 'o' || self->vtbl->readByte(self) != 'a' || self->vtbl->readByte(self) != 'f')
		return false;
	self->bufferBits = self->buffer = 0;
	self->totalSamples = QOADecoder_ReadBits(self, 32);
	if (self->totalSamples <= 0)
		return false;
	self->base.frameHeader = QOADecoder_ReadBits(self, 32);
	if (self->base.frameHeader <= 0)
		return false;
	self->positionSamples = 0;
	int channels = QOABase_GetChannels(&self->base);
	return channels > 0 && channels <= 8 && QOABase_GetSampleRate(&self->base) > 0;
}

int QOADecoder_GetTotalSamples(const QOADecoder *self)
{
	return self->totalSamples;
}

static int QOADecoder_GetMaxFrameBytes(const QOADecoder *self)
{
	return 8 + QOABase_GetChannels(&self->base) * 2064;
}

static bool QOADecoder_ReadLMS(QOADecoder *self, int *result)
{
	for (int i = 0; i < 4; i++) {
		int hi = self->vtbl->readByte(self);
		if (hi < 0)
			return false;
		int lo = self->vtbl->readByte(self);
		if (lo < 0)
			return false;
		result[i] = ((hi ^ 128) - 128) << 8 | lo;
	}
	return true;
}

int QOADecoder_ReadFrame(QOADecoder *self, int16_t *samples)
{
	if (self->positionSamples > 0 && QOADecoder_ReadBits(self, 32) != self->base.frameHeader)
		return -1;
	int samplesCount = QOADecoder_ReadBits(self, 16);
	if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > self->totalSamples - self->positionSamples)
		return -1;
	int channels = QOABase_GetChannels(&self->base);
	int slices = (samplesCount + 19) / 20;
	if (QOADecoder_ReadBits(self, 16) != 8 + channels * (16 + slices * 8))
		return -1;
	LMS lmses[8];
	for (int c = 0; c < channels; c++) {
		if (!QOADecoder_ReadLMS(self, lmses[c].history) || !QOADecoder_ReadLMS(self, lmses[c].weights))
			return -1;
	}
	for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex += 20) {
		for (int c = 0; c < channels; c++) {
			int scaleFactor = QOADecoder_ReadBits(self, 4);
			if (scaleFactor < 0)
				return -1;
			scaleFactor = QOABase_SCALE_FACTORS[scaleFactor];
			int sampleOffset = sampleIndex * channels + c;
			for (int s = 0; s < 20; s++) {
				int quantized = QOADecoder_ReadBits(self, 3);
				if (quantized < 0)
					return -1;
				if (sampleIndex + s >= samplesCount)
					continue;
				int dequantized = QOABase_Dequantize(quantized, scaleFactor);
				int reconstructed = QOABase_Clamp(LMS_Predict(&lmses[c]) + dequantized, -32768, 32767);
				LMS_Update(&lmses[c], reconstructed, dequantized);
				samples[sampleOffset] = (int16_t) reconstructed;
				sampleOffset += channels;
			}
		}
	}
	self->positionSamples += samplesCount;
	return samplesCount;
}

void QOADecoder_SeekToSample(QOADecoder *self, int position)
{
	int frame = position / 5120;
	self->vtbl->seekToByte(self, frame == 0 ? 12 : 8 + frame * QOADecoder_GetMaxFrameBytes(self));
	self->positionSamples = frame * 5120;
}

bool QOADecoder_IsEnd(const QOADecoder *self)
{
	return self->positionSamples >= self->totalSamples;
}
