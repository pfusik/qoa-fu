# Generated automatically with "fut". Do not edit.
import abc
import array

class _LMS:
	"""Least Mean Squares Filter."""

	def __init__(self):
		self._history = array.array("i", [ 0 ]) * 4
		self._weights = array.array("i", [ 0 ]) * 4
	_history: array.array
	_weights: array.array

	def _assign(self, source: "_LMS") -> None:
		self._history[0:4] = source._history[0:4]
		self._weights[0:4] = source._weights[0:4]

	def _predict(self) -> int:
		return (self._history[0] * self._weights[0] + self._history[1] * self._weights[1] + self._history[2] * self._weights[2] + self._history[3] * self._weights[3]) >> 13

	def _update(self, sample: int, residual: int) -> None:
		delta: int = residual >> 4
		self._weights[0] += -delta if self._history[0] < 0 else delta
		self._weights[1] += -delta if self._history[1] < 0 else delta
		self._weights[2] += -delta if self._history[2] < 0 else delta
		self._weights[3] += -delta if self._history[3] < 0 else delta
		self._history[0] = self._history[1]
		self._history[1] = self._history[2]
		self._history[2] = self._history[3]
		self._history[3] = sample

class QOABase(abc.ABC):
	"""Common part of the "Quite OK Audio" format encoder and decoder."""
	_frame_header: int

	MAX_CHANNELS = 8
	"""Maximum number of channels supported by the format."""

	def get_channels(self) -> int:
		"""Returns the number of audio channels."""
		return self._frame_header >> 24

	def get_sample_rate(self) -> int:
		"""Returns the sample rate in Hz."""
		return self._frame_header & 16777215

	_SLICE_SAMPLES = 20

	_MAX_FRAME_SLICES = 256

	MAX_FRAME_SAMPLES = 5120
	"""Maximum number of samples per frame."""

	def _get_frame_bytes(self, sample_count: int) -> int:
		slices: int = int((sample_count + 19) / 20)
		return 8 + self.get_channels() * (16 + slices * 8)

	_SCALE_FACTORS = array.array("h", [ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ])

	@staticmethod
	def _dequantize(quantized: int, scale_factor: int) -> int:
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

	@abc.abstractmethod
	def _write_long(self, l: int) -> bool:
		"""Writes the 64-bit integer in big endian order.

		Returns `True` on success.

		:param l: The integer to be written to the QOA stream.
		"""
	_l_m_ses: list[_LMS]

	def write_header(self, total_samples: int, channels: int, sample_rate: int) -> bool:
		"""Writes the file header.

		Returns `True` on success.

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
		magic: int = 1903124838
		return self._write_long(magic << 32 | total_samples)

	def _write_l_m_s(self, a: array.array) -> bool:
		a0: int = a[0]
		a1: int = a[1]
		a2: int = a[2]
		return self._write_long(a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | (a[3] & 65535))

	def write_frame(self, samples: array.array, samples_count: int) -> bool:
		"""Encodes and writes a frame.

		:param samples: PCM samples: `samplesCount * channels` elements.
		:param samples_count: Number of samples per channel.
		"""
		if samples_count <= 0 or samples_count > 5120:
			return False
		header: int = self._frame_header
		if not self._write_long(header << 32 | samples_count << 16 | self._get_frame_bytes(samples_count)):
			return False
		channels: int = self.get_channels()
		for c in range(channels):
			if not self._write_l_m_s(self._l_m_ses[c]._history) or not self._write_l_m_s(self._l_m_ses[c]._weights):
				return False
		lms: _LMS = _LMS()
		best_l_m_s: _LMS = _LMS()
		last_scale_factors: bytearray = bytearray(8)
		for sample_index in range(0, samples_count, 20):
			slice_samples: int = min(samples_count - sample_index, 20)
			for c in range(channels):
				best_rank: int = 9223372036854775807
				best_slice: int = 0
				for scale_factor_delta in range(16):
					scale_factor: int = (last_scale_factors[c] + scale_factor_delta) & 15
					lms._assign(self._l_m_ses[c])
					reciprocal: int = QOAEncoder._WRITE_FRAME_RECIPROCALS[scale_factor]
					slice: int = scale_factor
					current_rank: int = 0
					for s in range(slice_samples):
						sample: int = samples[(sample_index + s) * channels + c]
						predicted: int = lms._predict()
						residual: int = sample - predicted
						scaled: int = (residual * reciprocal + 32768) >> 16
						if scaled != 0:
							scaled += 1 if scaled < 0 else -1
						if residual != 0:
							scaled += 1 if residual > 0 else -1
						quantized: int = QOAEncoder._WRITE_FRAME_QUANT_TAB[8 + min(max(scaled, -8), 8)]
						dequantized: int = QOAEncoder._dequantize(quantized, QOAEncoder._SCALE_FACTORS[scale_factor])
						reconstructed: int = min(max(predicted + dequantized, -32768), 32767)
						error: int = sample - reconstructed
						current_rank += error * error
						weights_penalty: int = ((lms._weights[0] * lms._weights[0] + lms._weights[1] * lms._weights[1] + lms._weights[2] * lms._weights[2] + lms._weights[3] * lms._weights[3]) >> 18) - 2303
						if weights_penalty > 0:
							current_rank += weights_penalty
						if current_rank >= best_rank:
							break
						lms._update(reconstructed, dequantized)
						slice = slice << 3 | quantized
					if current_rank < best_rank:
						best_rank = current_rank
						best_slice = slice
						best_l_m_s._assign(lms)
				self._l_m_ses[c]._assign(best_l_m_s)
				best_slice <<= (20 - slice_samples) * 3
				last_scale_factors[c] = best_slice >> 60
				if not self._write_long(best_slice):
					return False
		return True

	_WRITE_FRAME_RECIPROCALS = array.array("i", [ 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 ])

	_WRITE_FRAME_QUANT_TAB = bytes([ 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
		6 ])

class QOADecoder(QOABase):
	"""Decoder of the "Quite OK Audio" format."""

	@abc.abstractmethod
	def _read_byte(self) -> int:
		"""Reads a byte from the stream.

		Returns the unsigned byte value or -1 on EOF."""

	@abc.abstractmethod
	def _seek_to_byte(self, position: int) -> None:
		"""Seeks the stream to the given position.

		:param position: File offset in bytes.
		"""
	_buffer: int
	_buffer_bits: int

	def _read_bits(self, bits: int) -> int:
		while self._buffer_bits < bits:
			b: int = self._read_byte()
			if b < 0:
				return -1
			self._buffer = self._buffer << 8 | b
			self._buffer_bits += 8
		self._buffer_bits -= bits
		result: int = self._buffer >> self._buffer_bits
		self._buffer &= (1 << self._buffer_bits) - 1
		return result
	_total_samples: int
	_position_samples: int

	def read_header(self) -> bool:
		"""Reads the file header.

		Returns `True` if the header is valid."""
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
		channels: int = self.get_channels()
		return channels > 0 and channels <= 8 and self.get_sample_rate() > 0

	def get_total_samples(self) -> int:
		"""Returns the file length in samples per channel."""
		return self._total_samples

	def _get_max_frame_bytes(self) -> int:
		return 8 + self.get_channels() * 2064

	def _read_l_m_s(self, result: array.array) -> bool:
		for i in range(4):
			hi: int = self._read_byte()
			if hi < 0:
				return False
			lo: int = self._read_byte()
			if lo < 0:
				return False
			result[i] = ((hi ^ 128) - 128) << 8 | lo
		return True

	def read_frame(self, samples: array.array) -> int:
		"""Reads and decodes a frame.

		Returns the number of samples per channel.

		:param samples: PCM samples.
		"""
		if self._position_samples > 0 and self._read_bits(32) != self._frame_header:
			return -1
		samples_count: int = self._read_bits(16)
		if samples_count <= 0 or samples_count > 5120 or samples_count > self._total_samples - self._position_samples:
			return -1
		channels: int = self.get_channels()
		slices: int = int((samples_count + 19) / 20)
		if self._read_bits(16) != 8 + channels * (16 + slices * 8):
			return -1
		lmses: list[_LMS] = [ _LMS() for _ in range(8) ]
		for c in range(channels):
			if not self._read_l_m_s(lmses[c]._history) or not self._read_l_m_s(lmses[c]._weights):
				return -1
		for sample_index in range(0, samples_count, 20):
			for c in range(channels):
				scale_factor: int = self._read_bits(4)
				if scale_factor < 0:
					return -1
				scale_factor = QOADecoder._SCALE_FACTORS[scale_factor]
				sample_offset: int = sample_index * channels + c
				for s in range(20):
					quantized: int = self._read_bits(3)
					if quantized < 0:
						return -1
					if sample_index + s >= samples_count:
						continue
					dequantized: int = QOADecoder._dequantize(quantized, scale_factor)
					reconstructed: int = min(max(lmses[c]._predict() + dequantized, -32768), 32767)
					lmses[c]._update(reconstructed, dequantized)
					samples[sample_offset] = reconstructed
					sample_offset += channels
		self._position_samples += samples_count
		return samples_count

	def seek_to_sample(self, position: int) -> None:
		"""Seeks to the given time offset.

		Requires the input stream to be seekable with `SeekToByte`.

		:param position: Position from the beginning of the file.
		"""
		frame: int = int(position / 5120)
		self._seek_to_byte(12 if frame == 0 else 8 + frame * self._get_max_frame_bytes())
		self._position_samples = frame * 5120

	def is_end(self) -> bool:
		"""Returns `True` if all frames have been read."""
		return self._position_samples >= self._total_samples
