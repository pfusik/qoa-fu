// Generated automatically with "cito". Do not edit.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct QOABase QOABase;
typedef struct QOAEncoder QOAEncoder;
typedef struct QOADecoder QOADecoder;

/**
 * Maximum number of channels supported by the format.
 */
#define QOABase_MAX_CHANNELS 8

/**
 * Returns the number of audio channels.
 * @param self This <code>QOABase</code>.
 */
int QOABase_GetChannels(const QOABase *self);

/**
 * Returns the sample rate in Hz.
 * @param self This <code>QOABase</code>.
 */
int QOABase_GetSampleRate(const QOABase *self);

/**
 * Maximum number of samples per frame.
 */
#define QOABase_MAX_FRAME_SAMPLES 5120

/**
 * Writes the file header.
 * Returns <code>true</code> on success.
 * @param self This <code>QOAEncoder</code>.
 * @param totalSamples File length in samples per channel.
 * @param channels Number of audio channels.
 * @param sampleRate Sample rate in Hz.
 */
bool QOAEncoder_WriteHeader(QOAEncoder *self, int totalSamples, int channels, int sampleRate);

/**
 * Encodes and writes a frame.
 * @param self This <code>QOAEncoder</code>.
 * @param samples PCM samples: <code>samplesCount * channels</code> elements.
 * @param samplesCount Number of samples per channel.
 */
bool QOAEncoder_WriteFrame(QOAEncoder *self, int16_t const *samples, int samplesCount);

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
 * Reads and decodes a frame.
 * Returns the number of samples per channel.
 * @param self This <code>QOADecoder</code>.
 * @param samples PCM samples.
 */
int QOADecoder_ReadFrame(QOADecoder *self, int16_t *samples);

/**
 * Seeks to the given time offset.
 * Requires the input stream to be seekable with <code>SeekToByte</code>.
 * @param self This <code>QOADecoder</code>.
 * @param position Position from the beginning of the file.
 */
void QOADecoder_SeekToSample(QOADecoder *self, int position);

/**
 * Returns <code>true</code> if all frames have been read.
 * @param self This <code>QOADecoder</code>.
 */
bool QOADecoder_IsEnd(const QOADecoder *self);

#ifdef __cplusplus
}
#endif
