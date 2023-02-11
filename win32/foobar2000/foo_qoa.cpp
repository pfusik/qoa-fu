// foo_qoa.cpp - "Quite OK Audio" plugin for foobar2000
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

#include "foobar2000/SDK/foobar2000.h"
#include "QOA.hpp"

class FooQOADecoder : public QOADecoder
{
	service_ptr_t<file> m_file;
	abort_callback *p_abort = nullptr;

protected:

	int readByte() override
	{
		uint8_t b;
		if (m_file->read(&b, 1, *p_abort) != 1)
			return -1;
		return b;
	}

	void seekToByte(int position) override
	{
		m_file->seek(position, *p_abort);
	}

public:

	service_ptr_t<file> &get_file()
	{
		return m_file;
	}

	void set_abort(abort_callback &p_abort)
	{
		this->p_abort = &p_abort;
	}
};

class input_qoa : public input_stubs
{
	FooQOADecoder qoa;
	int16_t samples[QOADecoder::maxFrameSamples * QOADecoder::maxChannels];

public:

	static bool g_is_our_content_type(const char *p_content_type)
	{
		return false;
	}

	static bool g_is_our_path(const char * p_path, const char * p_extension)
	{
		return stricmp_utf8(p_extension, "qoa") == 0;
	}

	static GUID g_get_guid()
	{
		static const GUID guid =
			{ 0xf99f4089, 0xa880, 0x4a94, {0x95, 0x6c, 0x31, 0x6f, 0x16, 0x03, 0xf6, 0x40} };
		return guid;
	}

	static const char *g_get_name()
	{
		return "QOA";
	}

	void open(service_ptr_t<file> p_filehint, const char *p_path, t_input_open_reason p_reason, abort_callback &p_abort)
	{
		if (p_filehint.is_empty())
			filesystem::g_open(p_filehint, p_path, filesystem::open_mode_read, p_abort);
		qoa.get_file() = p_filehint;
		qoa.set_abort(p_abort);
		if (!qoa.readHeader())
			throw exception_io_unsupported_format();
	}

	void get_info(file_info &p_info, abort_callback &p_abort) const
	{
		int sampleRate = qoa.getSampleRate();
		p_info.set_length((double) qoa.getTotalSamples() / sampleRate);
		p_info.info_set_int("channels", qoa.getChannels());
		p_info.info_set_int("samplerate", sampleRate);
		p_info.info_set_bitrate((sampleRate * qoa.getChannels() * 32 + 5000) / 10000); // 3.2 bits per sample
	}

	t_filestats2 get_stats2(uint32_t f, abort_callback &p_abort)
	{
		return qoa.get_file()->get_stats2_(f, p_abort);
	}

	void decode_initialize(unsigned p_flags, abort_callback &p_abort) const
	{
	}

	bool decode_run(audio_chunk &p_chunk, abort_callback &p_abort)
	{
		if (qoa.isEnd())
			return false;
		qoa.set_abort(p_abort);
		int n = qoa.readFrame(samples);
		if (n <= 0)
			return false;
		int channels = qoa.getChannels();
		p_chunk.set_data_int16(samples, n, channels, qoa.getSampleRate(), audio_chunk::g_guess_channel_config(channels));
		return true;
	}

	void decode_seek(double p_seconds, abort_callback &p_abort)
	{
		qoa.set_abort(p_abort);
		qoa.seekToSample(static_cast<int>(p_seconds * qoa.getSampleRate()));
	}

	bool decode_can_seek()
	{
		return qoa.get_file()->can_seek();
	}

	void retag(const file_info &p_info, abort_callback &p_abort)
	{
		throw exception_tagging_unsupported();
	}

	void remove_tags(abort_callback &p_abort)
	{
		throw exception_tagging_unsupported();
	}
};

static input_singletrack_factory_t<input_qoa> g_input_qoa_factory;

DECLARE_FILE_TYPE("Quite OK Audio","*.QOA");

DECLARE_COMPONENT_VERSION("Quite OK Audio (QOA) Decoder", "0.1.0", "(C) 2023 Piotr Fusik");
