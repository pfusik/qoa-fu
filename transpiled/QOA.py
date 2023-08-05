# Generated automatically with "fut". Do not edit.
import array

class _LMS:
	"""Least Mean Squares Filter."""

	def __init__(self):
		self._history = array.array("i", [ 0 ]) * 4
		self._weights = array.array("i", [ 0 ]) * 4

	def _assign(self, source):
		self._history[0:4] = source._history[0:4]
		self._weights[0:4] = source._weights[0:4]

	def _predict(self):
		return (self._history[0] * self._weights[0] + self._history[1] * self._weights[1] + self._history[2] * self._weights[2] + self._history[3] * self._weights[3]) >> 13

	def _update(self, sample, residual):
		delta = residual >> 4
		self._weights[0] += -delta if self._history[0] < 0 else delta
		self._weights[1] += -delta if self._history[1] < 0 else delta
		self._weights[2] += -delta if self._history[2] < 0 else delta
		self._weights[3] += -delta if self._history[3] < 0 else delta
		self._history[0] = self._history[1]
		self._history[1] = self._history[2]
		self._history[2] = self._history[3]
		self._history[3] = sample

class QOABase:
	"""Common part of the "Quite OK Audio" format encoder and decoder."""

	@staticmethod
	def _clamp(value, min, max):
		return min if value < min else max if value > max else value

	MAX_CHANNELS = 8
	"""Maximum number of channels supported by the format."""

	def get_channels(self):
		"""Returns the number of audio channels."""
		return self._frame_header >> 24

	def get_sample_rate(self):
		"""Returns the sample rate in Hz."""
		return self._frame_header & 16777215

	_SLICE_SAMPLES = 20

	_MAX_FRAME_SLICES = 256

	MAX_FRAME_SAMPLES = 5120
	"""Maximum number of samples per frame."""

	def _get_frame_bytes(self, sample_count):
		slices = int((sample_count + 19) / 20)
		return 8 + self.get_channels() * (16 + slices * 8)

	_SCALE_FACTORS = array.array("h", [ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ])

	@staticmethod
	def _dequantize(quantized, scale_factor):
		match quantized >> 1:
			case 0:
				dequantized = (scale_factor * 3 + 2) >> 2
			case 1:
				dequantized = (scale_factor * 5 + 1) >> 1
			case 2:
				dequantized = (scale_factor * 9 + 1) >> 1
			case _:
				dequantized = scale_factor * 7
		return -dequantized if (quantized & 1) != 0 else dequantized

class QOAEncoder(QOABase):
	"""Encoder of the "Quite OK Audio" format."""

	def __init__(self):
		self._l_m_ses = [ _LMS() for _ in range(8) ]

	def write_header(self, total_samples, channels, sample_rate):
		"""Writes the file header.

		Returns `true` on success.

		:param total_samples: File length in samples per channel.
		:param channels: Number of audio channels.
		:param sample_rate: Sample rate in Hz.
		"""
		if total_samples <= 0 or channels <= 0 or channels > 8 or sample_rate <= 0 or sample_rate >= 16777216:
			return False
		self._frame_header = channels << 24 | sample_rate
		for c in range(channels):
			self._l_m_ses[c]._history[:] = array.array("i", [ 0 ]) * 4
			self._l_m_ses[c]._weights[0] = 0
			self._l_m_ses[c]._weights[1] = 0
			self._l_m_ses[c]._weights[2] = -8192
			self._l_m_ses[c]._weights[3] = 16384
		magic = 1903124838
		return self._write_long(magic << 32 | total_samples)

	def _write_l_m_s(self, a):
		a0 = a[0]
		a1 = a[1]
		a2 = a[2]
		return self._write_long(a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | (a[3] & 65535))

	def write_frame(self, samples, samples_count):
		"""Encodes and writes a frame.

		:param samples: PCM samples: `samplesCount * channels` elements.
		:param samples_count: Number of samples per channel.
		"""
		if samples_count <= 0 or samples_count > 5120:
			return False
		header = self._frame_header
		if not self._write_long(header << 32 | samples_count << 16 | self._get_frame_bytes(samples_count)):
			return False
		channels = self.get_channels()
		for c in range(channels):
			if not self._write_l_m_s(self._l_m_ses[c]._history) or not self._write_l_m_s(self._l_m_ses[c]._weights):
				return False
		lms = _LMS()
		best_l_m_s = _LMS()
		for sample_index in range(0, samples_count, 20):
			slice_samples = samples_count - sample_index
			if slice_samples > 20:
				slice_samples = 20
			for c in range(channels):
				best_error = 9223372036854775807
				best_slice = 0
				for scale_factor in range(16):
					lms._assign(self._l_m_ses[c])
					reciprocal = QOAEncoder._WRITE_FRAME_RECIPROCALS[scale_factor]
					slice = scale_factor
					current_error = 0
					for s in range(slice_samples):
						sample = samples[(sample_index + s) * channels + c]
						predicted = lms._predict()
						residual = sample - predicted
						scaled = (residual * reciprocal + 32768) >> 16
						if scaled != 0:
							scaled += 1 if scaled < 0 else -1
						if residual != 0:
							scaled += 1 if residual > 0 else -1
						quantized = QOAEncoder._WRITE_FRAME_QUANT_TAB[8 + QOAEncoder._clamp(scaled, -8, 8)]
						dequantized = QOAEncoder._dequantize(quantized, QOAEncoder._SCALE_FACTORS[scale_factor])
						reconstructed = QOAEncoder._clamp(predicted + dequantized, -32768, 32767)
						error = sample - reconstructed
						current_error += error * error
						if current_error >= best_error:
							break
						lms._update(reconstructed, dequantized)
						slice = slice << 3 | quantized
					if current_error < best_error:
						best_error = current_error
						best_slice = slice
						best_l_m_s._assign(lms)
				self._l_m_ses[c]._assign(best_l_m_s)
				best_slice <<= (20 - slice_samples) * 3
				if not self._write_long(best_slice):
					return False
		return True

	_WRITE_FRAME_RECIPROCALS = array.array("i", [ 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 ])

	_WRITE_FRAME_QUANT_TAB = bytes([ 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
		6 ])

class QOADecoder(QOABase):
	"""Decoder of the "Quite OK Audio" format."""

	def _read_bits(self, bits):
		while self._buffer_bits < bits:
			b = self._read_byte()
			if b < 0:
				return -1
			self._buffer = self._buffer << 8 | b
			self._buffer_bits += 8
		self._buffer_bits -= bits
		result = self._buffer >> self._buffer_bits
		self._buffer &= (1 << self._buffer_bits) - 1
		return result

	def read_header(self):
		"""Reads the file header.

		Returns `true` if the header is valid."""
		if self._read_byte() != 113 or self._read_byte() != 111 or self._read_byte() != 97 or self._read_byte() != 102:
			return False
		self._buffer_bits = self._buffer = 0
		self._total_samples = self._read_bits(32)
		if self._total_samples <= 0:
			return False
		self._frame_header = self._read_bits(32)
		if self._frame_header <= 0:
			return False
		self._position_samples = 0
		channels = self.get_channels()
		return channels > 0 and channels <= 8 and self.get_sample_rate() > 0

	def get_total_samples(self):
		"""Returns the file length in samples per channel."""
		return self._total_samples

	def _get_max_frame_bytes(self):
		return 8 + self.get_channels() * 2064

	def _read_l_m_s(self, result):
		for i in range(4):
			hi = self._read_byte()
			if hi < 0:
				return False
			lo = self._read_byte()
			if lo < 0:
				return False
			result[i] = ((hi ^ 128) - 128) << 8 | lo
		return True

	def read_frame(self, samples):
		"""Reads and decodes a frame.

		Returns the number of samples per channel.

		:param samples: PCM samples.
		"""
		if self._position_samples > 0 and self._read_bits(32) != self._frame_header:
			return -1
		samples_count = self._read_bits(16)
		if samples_count <= 0 or samples_count > 5120 or samples_count > self._total_samples - self._position_samples:
			return -1
		channels = self.get_channels()
		slices = int((samples_count + 19) / 20)
		if self._read_bits(16) != 8 + channels * (16 + slices * 8):
			return -1
		lmses = [ _LMS() for _ in range(8) ]
		for c in range(channels):
			if not self._read_l_m_s(lmses[c]._history) or not self._read_l_m_s(lmses[c]._weights):
				return -1
		for sample_index in range(0, samples_count, 20):
			for c in range(channels):
				scale_factor = self._read_bits(4)
				if scale_factor < 0:
					return -1
				scale_factor = QOADecoder._SCALE_FACTORS[scale_factor]
				sample_offset = sample_index * channels + c
				for s in range(20):
					quantized = self._read_bits(3)
					if quantized < 0:
						return -1
					if sample_index + s >= samples_count:
						continue
					dequantized = QOADecoder._dequantize(quantized, scale_factor)
					reconstructed = QOADecoder._clamp(lmses[c]._predict() + dequantized, -32768, 32767)
					lmses[c]._update(reconstructed, dequantized)
					samples[sample_offset] = reconstructed
					sample_offset += channels
		self._position_samples += samples_count
		return samples_count

	def seek_to_sample(self, position):
		"""Seeks to the given time offset.

		Requires the input stream to be seekable with `SeekToByte`.

		:param position: Position from the beginning of the file.
		"""
		frame = int(position / 5120)
		self._seek_to_byte(12 if frame == 0 else 8 + frame * self._get_max_frame_bytes())
		self._position_samples = frame * 5120

	def is_end(self):
		"""Returns `true` if all frames have been read."""
		return self._position_samples >= self._total_samples
