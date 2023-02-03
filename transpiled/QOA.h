// Generated automatically with "cito". Do not edit.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct QOADecoder QOADecoder;

QOADecoder *QOADecoder_New(void);
void QOADecoder_Delete(QOADecoder *self);

/**
 * Reads the file header.
 * Returns <code>true</code> if the header is valid.
 * @param self This <code>QOADecoder</code>.
 */
bool QOADecoder_ReadHeader(QOADecoder *self);

/**
 * Returns the file length in samples per channel.
 * @param self This <code>QOADecoder</code>.
 */
int QOADecoder_GetTotalSamples(const QOADecoder *self);

/**
 * Maximum number of channels supported by the format.
 */
#define QOADecoder_MAX_CHANNELS 8

/**
 * Returns the number of audio channels.
 * @param self This <code>QOADecoder</code>.
 */
int QOADecoder_GetChannels(const QOADecoder *self);

/**
 * Returns the sample rate in Hz.
 * @param self This <code>QOADecoder</code>.
 */
int QOADecoder_GetSampleRate(const QOADecoder *self);

/**
 * Number of samples per frame.
 */
#define QOADecoder_FRAME_SAMPLES 5120

/**
 * Reads and decodes a frame.
 * Returns the number of samples per channel.
 * @param self This <code>QOADecoder</code>.
 * @param output PCM samples.
 */
int QOADecoder_ReadFrame(QOADecoder *self, int16_t *output);

/**
 * Returns <code>true</code> if all frames have been read.
 * @param self This <code>QOADecoder</code>.
 */
bool QOADecoder_IsEnd(const QOADecoder *self);

#ifdef __cplusplus
}
#endif
