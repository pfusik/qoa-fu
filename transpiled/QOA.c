// Generated automatically with "cito". Do not edit.
#include <stdlib.h>
#include "QOA.h"
typedef struct LMS LMS;

/**
 * Least Mean Squares Filter.
 */
struct LMS {
	int history[4];
	int weights[4];
};

static int LMS_Predict(const LMS *self);

static void LMS_Update(LMS *self, int sample, int residual);

typedef struct {
	int (*readByte)(QOADecoder *self);
	void (*seekToByte)(QOADecoder *self, int position);
} QOADecoderVtbl;
/**
 * Decoder of the "Quite OK Audio" format.
 */
struct QOADecoder {
	const QOADecoderVtbl *vtbl;
	int buffer;
	int bufferBits;
	int totalSamples;
	int expectedFrameHeader;
	int positionSamples;
};
static void QOADecoder_Construct(QOADecoder *self);

static int QOADecoder_ReadBits(QOADecoder *self, int bits);

static int QOADecoder_GetMaxFrameBytes(const QOADecoder *self);

static int QOADecoder_Clamp(int value, int min, int max);

static bool QOADecoder_ReadLMS(QOADecoder *self, int *result);

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

static void QOADecoder_Construct(QOADecoder *self)
{
}

QOADecoder *QOADecoder_New(void)
{
	QOADecoder *self = (QOADecoder *) malloc(sizeof(QOADecoder));
	if (self != NULL)
		QOADecoder_Construct(self);
	return self;
}

void QOADecoder_Delete(QOADecoder *self)
{
	free(self);
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
	self->expectedFrameHeader = QOADecoder_ReadBits(self, 32);
	if (self->expectedFrameHeader <= 0)
		return false;
	self->positionSamples = 0;
	int channels = QOADecoder_GetChannels(self);
	return channels > 0 && channels <= 8 && QOADecoder_GetSampleRate(self) > 0;
}

int QOADecoder_GetTotalSamples(const QOADecoder *self)
{
	return self->totalSamples;
}

int QOADecoder_GetChannels(const QOADecoder *self)
{
	return self->expectedFrameHeader >> 24;
}

int QOADecoder_GetSampleRate(const QOADecoder *self)
{
	return self->expectedFrameHeader & 16777215;
}

static int QOADecoder_GetMaxFrameBytes(const QOADecoder *self)
{
	return 8 + QOADecoder_GetChannels(self) * 2064;
}

static int QOADecoder_Clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
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
	if (self->positionSamples > 0 && QOADecoder_ReadBits(self, 32) != self->expectedFrameHeader)
		return -1;
	int samplesCount = QOADecoder_ReadBits(self, 16);
	if (samplesCount <= 0 || samplesCount > 5120 || samplesCount > self->totalSamples - self->positionSamples)
		return -1;
	int channels = QOADecoder_GetChannels(self);
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
			static const uint16_t SCALE_FACTORS[16] = { 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 };
			scaleFactor = SCALE_FACTORS[scaleFactor];
			int sampleOffset = sampleIndex * channels + c;
			for (int s = 0; s < 20; s++) {
				int quantized = QOADecoder_ReadBits(self, 3);
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
				int reconstructed = QOADecoder_Clamp(LMS_Predict(&lmses[c]) + dequantized, -32768, 32767);
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
