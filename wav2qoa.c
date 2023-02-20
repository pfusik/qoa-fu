// wav2qoa.c - command-line converter between WAV and QOA formats
//
// Copyright (C) 2023 Piotr Fusik
//
// MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "QOA.c"

static void usage(void)
{
	puts(
		"Usage: wav2qoa [OPTIONS] INPUTFILE...\n"
		"INPUTFILE must be in WAV or QOA format.\n"
		"Options:\n"
		"-o FILE  --output=FILE   Set output file name\n"
		"-h       --help          Display this information\n"
		"-v       --version       Display version information"
	);
}

static int chunk(const char *s)
{
	return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
}

static int readWavShort(FILE *f)
{
	int lo = getc(f);
	return lo | getc(f) << 8;
}

static int readWavInt(FILE *f)
{
	uint8_t bytes[4];
	if (fread(bytes, 4, 1, f) != 1)
		return -1;
	return bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24;
}

static FILE *qoa_stream;

static bool writeLong(QOAEncoder *self, int64_t l)
{
	return putc((uint8_t) (l >> 56), qoa_stream) >= 0
		&& putc((uint8_t) (l >> 48), qoa_stream) >= 0
		&& putc((uint8_t) (l >> 40), qoa_stream) >= 0
		&& putc((uint8_t) (l >> 32), qoa_stream) >= 0
		&& putc((uint8_t) (l >> 24), qoa_stream) >= 0
		&& putc((uint8_t) (l >> 16), qoa_stream) >= 0
		&& putc((uint8_t) (l >> 8), qoa_stream) >= 0
		&& putc((uint8_t) l, qoa_stream) >= 0;
}

static bool wav2qoa(const char *input_file, FILE *wav_stream, const char *output_file)
{
	// "RIFF" already read
	readWavInt(wav_stream);
	if (readWavInt(wav_stream) != chunk("WAVE")) {
		fprintf(stderr, "wav2qoa: %s: not a WAVE file\n", input_file);
		return false;
	}
	int chunkLength;
	int channels = 0;
	int sampleRate = 0;
	for (;;) {
		int chunkId = readWavInt(wav_stream);
		chunkLength = readWavInt(wav_stream);
		if (chunkLength < 0) {
			fprintf(stderr, "wav2qoa: %s: Invalid WAVE chunk\n", input_file);
			return false;
		}
		if (chunkId == chunk("fmt ")) {
			if (chunkLength != 16 || readWavShort(wav_stream) != 1) {
				fprintf(stderr, "wav2qoa: %s: Invalid WAVE file\n", input_file);
				return false;
			}
			channels = readWavShort(wav_stream);
			sampleRate = readWavInt(wav_stream);
			readWavInt(wav_stream);
			if (readWavShort(wav_stream) != channels * 2 || readWavShort(wav_stream) != 16) {
				fprintf(stderr, "wav2qoa: %s: Not 16 bits per sample\n", input_file);
				return false;
			}
		}
		else if (chunkId == chunk("data"))
			break;
		else if (fseek(wav_stream, chunkLength, SEEK_CUR) != 0) {
			perror(input_file);
			return false;
		}
	}
	if (channels <= 0) {
		fprintf(stderr, "wav2qoa: %s: Invalid WAVE file\n", input_file);
		return false;
	}

	qoa_stream = fopen(output_file, "wb");
	if (qoa_stream == NULL) {
		perror(output_file);
		return false;
	}
	QOAEncoder qoa;
	static const QOAEncoderVtbl vtbl = { writeLong };
	qoa.vtbl = &vtbl;
	chunkLength /= channels * 2;
	if (!QOAEncoder_WriteHeader(&qoa, chunkLength, channels, sampleRate)) {
		fprintf(stderr, "wav2qoa: %s: Unsupported WAVE format\n", input_file);
		fclose(qoa_stream);
		return false;
	}
	while (chunkLength > 0) {
		short samples[QOABase_MAX_FRAME_SAMPLES * QOABase_MAX_CHANNELS];
		int samplesCount = chunkLength;
		if (samplesCount > QOABase_MAX_FRAME_SAMPLES)
			samplesCount = QOABase_MAX_FRAME_SAMPLES;
		for (int i = 0; i < samplesCount * channels; i++) {
			int lo = getc(wav_stream);
			int hi = getc(wav_stream);
			if (lo < 0 || hi < 0) {
				fprintf(stderr, "wav2qoa: %s: Read error\n", input_file);
				fclose(qoa_stream);
				return false;
			}
			samples[i] = lo | hi << 8;
		}
		if (!QOAEncoder_WriteFrame(&qoa, samples, samplesCount)) {
			perror(output_file);
			fclose(qoa_stream);
			return false;
		}
		chunkLength -= samplesCount;
	}
	return fclose(qoa_stream) == 0;
}

static int readByte(QOADecoder *self)
{
	return getc(qoa_stream);
}

static bool writeWavShort(FILE *wav_stream, int x)
{
	return putc(x & 0xff, wav_stream) >= 0
		&& putc(x >> 8 & 0xff, wav_stream) >= 0;
}

static bool writeWavInt(FILE *wav_stream, int x)
{
	return writeWavShort(wav_stream, x)
		&& writeWavShort(wav_stream, x >> 16);
}

static bool writeWavHeader(FILE *wav_stream, const char *s, int x)
{
	return fwrite(s, strlen(s), 1, wav_stream) == 1 && writeWavInt(wav_stream, x);
}

static bool qoa2wav(const char *input_file, const char *output_file)
{
	QOADecoder qoa;
	static const QOADecoderVtbl vtbl = { readByte };
	qoa.vtbl = &vtbl;
	if (!QOADecoder_ReadHeader(&qoa)) {
		fprintf(stderr, "wav2qoa: %s: error loading\n", input_file);
		return false;
	}
	FILE *wav_stream = fopen(output_file, "wb");
	if (wav_stream == NULL) {
		perror(output_file);
		return false;
	}
	int channels = QOABase_GetChannels(&qoa.base);
	int blockSize = channels * 2;
	int nBytes = QOADecoder_GetTotalSamples(&qoa) * blockSize;
	if (writeWavHeader(wav_stream, "RIFF", 36 + nBytes)
		&& writeWavHeader(wav_stream, "WAVEfmt ", 16)
		&& writeWavShort(wav_stream, 1)
		&& writeWavShort(wav_stream, channels)
		&& writeWavInt(wav_stream, QOABase_GetSampleRate(&qoa.base))
		&& writeWavInt(wav_stream, QOABase_GetSampleRate(&qoa.base) * blockSize)
		&& writeWavShort(wav_stream, blockSize)
		&& writeWavShort(wav_stream, 16)
		&& writeWavHeader(wav_stream, "data", nBytes)) {
		while (!QOADecoder_IsEnd(&qoa)) {
			short samples[QOABase_MAX_FRAME_SAMPLES * QOABase_MAX_CHANNELS];
			int samplesCount = QOADecoder_ReadFrame(&qoa, samples) * channels;
			if (samplesCount < 0) {
				fprintf(stderr, "wav2qoa: %s: error decoding\n", input_file);
				fclose(wav_stream);
				return false;
			}
			for (int i = 0; i < samplesCount; i++) {
				if (!writeWavShort(wav_stream, samples[i])) {
					perror(output_file);
					fclose(wav_stream);
					return false;
				}
			}
		}
		if (fclose(wav_stream) == 0)
			return true;
	}
	perror(output_file);
	fclose(wav_stream);
	return false;
}

static bool process_file(const char *input_file, const char *output_file)
{
	FILE *f = fopen(input_file, "rb");
	if (f == NULL) {
		perror(input_file);
		return false;
	}
	int magic = readWavInt(f);

	char default_output_file[FILENAME_MAX];
	if (output_file == NULL) {
		const char *ext = magic == chunk("RIFF") ? "qoa" : "wav";
		if (snprintf(default_output_file, sizeof(default_output_file), "%s.%s", input_file, ext) >= sizeof(default_output_file)) {
			fclose(f);
			fprintf(stderr, "wav2qoa: %s: filename too long\n", input_file);
			return false;
		}
		output_file = default_output_file;
	}

	bool ok;
	if (magic == chunk("RIFF"))
		ok = wav2qoa(input_file, f, output_file);
	else if (magic == chunk("qoaf")) {
		qoa_stream = f;
		ok = fseek(f, 0, SEEK_SET) == 0 && qoa2wav(input_file, output_file);
	}
	else {
		fprintf(stderr, "wav2qoa: %s: unrecognized file format\n", input_file);
		ok = false;
	}
	fclose(f);
	return ok;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}
	const char *output_file = NULL;
	bool ok = true;
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] != '-') {
			ok &= process_file(arg, output_file);
			output_file = NULL;
		}
		else if (arg[1] == 'o' && arg[2] == '\0' && i + 1 < argc)
			output_file = argv[++i];
		else if (strncmp(arg, "--output=", 9) == 0)
			output_file = arg + 9;
		else if ((arg[1] == 'h' && arg[2] == '\0') || strcmp(arg, "--help") == 0)
			usage();
		else if ((arg[1] == 'v' && arg[2] == '\0') || strcmp(arg, "--version") == 0)
			puts("wav2qoa 0.3.0");
		else {
			fprintf(stderr, "wav2qoa: unknown option: %s\n", arg);
			return 1;
		}
	}
	return ok ? 0 : 1;
}
