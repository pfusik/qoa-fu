// Generated automatically with "fut". Do not edit.

/// Least Mean Squares Filter.
class LMS
{

	fileprivate let history = ArrayRef<Int>(repeating: 0, count: 4)

	fileprivate let weights = ArrayRef<Int>(repeating: 0, count: 4)

	fileprivate func assign(_ source : LMS)
	{
		self.history[0..<4] = source.history[0..<4]
		self.weights[0..<4] = source.weights[0..<4]
	}

	fileprivate func predict() -> Int
	{
		return (self.history[0] * self.weights[0] + self.history[1] * self.weights[1] + self.history[2] * self.weights[2] + self.history[3] * self.weights[3]) >> 13
	}

	fileprivate func update(_ sample : Int, _ residual : Int)
	{
		let delta : Int = residual >> 4
		self.weights[0] += self.history[0] < 0 ? -delta : delta
		self.weights[1] += self.history[1] < 0 ? -delta : delta
		self.weights[2] += self.history[2] < 0 ? -delta : delta
		self.weights[3] += self.history[3] < 0 ? -delta : delta
		self.history[0] = self.history[1]
		self.history[1] = self.history[2]
		self.history[2] = self.history[3]
		self.history[3] = sample
	}
}

/// Common part of the "Quite OK Audio" format encoder and decoder.
public class QOABase
{

	public static func clamp(_ value : Int, _ min : Int, _ max : Int) -> Int
	{
		return value < min ? min : value > max ? max : value
	}

	public var frameHeader : Int = 0

	/// Maximum number of channels supported by the format.
	public static let maxChannels = 8

	/// Returns the number of audio channels.
	public func getChannels() -> Int
	{
		return self.frameHeader >> 24
	}

	/// Returns the sample rate in Hz.
	public func getSampleRate() -> Int
	{
		return self.frameHeader & 16777215
	}

	public static let sliceSamples = 20

	public static let maxFrameSlices = 256

	/// Maximum number of samples per frame.
	public static let maxFrameSamples = 5120

	public func getFrameBytes(_ sampleCount : Int) -> Int
	{
		let slices : Int = (sampleCount + 19) / 20
		return 8 + getChannels() * (16 + slices * 8)
	}

	public static let scaleFactors = [Int16]([ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ])

	public static func dequantize(_ quantized : Int, _ scaleFactor : Int) -> Int
	{
		var dequantized : Int
		switch quantized >> 1 {
		case 0:
			dequantized = (scaleFactor * 3 + 2) >> 2
			break
		case 1:
			dequantized = (scaleFactor * 5 + 1) >> 1
			break
		case 2:
			dequantized = (scaleFactor * 9 + 1) >> 1
			break
		default:
			dequantized = scaleFactor * 7
			break
		}
		return quantized & 1 != 0 ? -dequantized : dequantized
	}
}

/// Encoder of the "Quite OK Audio" format.
public class QOAEncoder : QOABase
{

	/// Writes the 64-bit integer in big endian order.
	/// Returns `true` on success.
	/// - parameter l The integer to be written to the QOA stream.
	open func writeLong(_ l : Int64) -> Bool
	{
		preconditionFailure("Abstract method called")
	}

	private let lMSes = ArrayRef<LMS>(factory: LMS.init, count: 8)

	/// Writes the file header.
	/// Returns `true` on success.
	/// - parameter totalSamples File length in samples per channel.
	/// - parameter channels Number of audio channels.
	/// - parameter sampleRate Sample rate in Hz.
	public func writeHeader(_ totalSamples : Int, _ channels : Int, _ sampleRate : Int) -> Bool
	{
		if totalSamples <= 0 || channels <= 0 || channels > 8 || sampleRate <= 0 || sampleRate >= 16777216 {
			return false
		}
		self.frameHeader = channels << 24 | sampleRate
		for c in 0..<channels {
			self.lMSes[c].history.fill(0)
			self.lMSes[c].weights[0] = 0
			self.lMSes[c].weights[1] = 0
			self.lMSes[c].weights[2] = -8192
			self.lMSes[c].weights[3] = 16384
		}
		let magic : Int64 = 1903124838
		return writeLong(magic << 32 | Int64(totalSamples))
	}

	private func writeLMS(_ a : ArrayRef<Int>) -> Bool
	{
		let a0 : Int64 = Int64(a[0])
		let a1 : Int64 = Int64(a[1])
		let a2 : Int64 = Int64(a[2])
		return writeLong(a0 << 48 | (a1 & 65535) << 32 | (a2 & 65535) << 16 | Int64(a[3] & 65535))
	}

	/// Encodes and writes a frame.
	/// - parameter samples PCM samples: `samplesCount * channels` elements.
	/// - parameter samplesCount Number of samples per channel.
	public func writeFrame(_ samples : ArrayRef<Int16>, _ samplesCount : Int) -> Bool
	{
		if samplesCount <= 0 || samplesCount > 5120 {
			return false
		}
		let header : Int64 = Int64(self.frameHeader)
		if !writeLong(header << 32 | Int64(samplesCount << 16) | Int64(getFrameBytes(samplesCount))) {
			return false
		}
		let channels : Int = getChannels()
		for c in 0..<channels {
			if !writeLMS(self.lMSes[c].history) || !writeLMS(self.lMSes[c].weights) {
				return false
			}
		}
		let lms = LMS()
		let bestLMS = LMS()
		var lastScaleFactors = [UInt8](repeating: 0, count: 8)
		for sampleIndex in stride(from: 0, to: samplesCount, by: 20) {
			var sliceSamples : Int = samplesCount - sampleIndex
			if sliceSamples > 20 {
				sliceSamples = 20
			}
			for c in 0..<channels {
				var bestError : Int64 = 9223372036854775807
				var bestSlice : Int64 = 0
				for scaleFactorDelta in 0..<16 {
					let scaleFactor : Int = (Int(lastScaleFactors[c]) + scaleFactorDelta) & 15
					lms.assign(self.lMSes[c])
					let reciprocal : Int = QOAEncoder.writeFrameReciprocals[scaleFactor]
					var slice : Int64 = Int64(scaleFactor)
					var currentError : Int64 = 0
					for s in 0..<sliceSamples {
						let sample : Int = Int(samples[(sampleIndex + s) * channels + c])
						let predicted : Int = lms.predict()
						let residual : Int = sample - predicted
						var scaled : Int = (residual * reciprocal + 32768) >> 16
						if scaled != 0 {
							scaled += scaled < 0 ? 1 : -1
						}
						if residual != 0 {
							scaled += residual > 0 ? 1 : -1
						}
						let quantized : Int = Int(QOAEncoder.writeFrameQuantTab[8 + QOAEncoder.clamp(scaled, -8, 8)])
						let dequantized : Int = QOAEncoder.dequantize(quantized, Int(QOAEncoder.scaleFactors[scaleFactor]))
						let reconstructed : Int = QOAEncoder.clamp(predicted + dequantized, -32768, 32767)
						let error : Int64 = Int64(sample - reconstructed)
						currentError += error * error
						if currentError >= bestError {
							break
						}
						lms.update(reconstructed, dequantized)
						slice = slice << 3 | Int64(quantized)
					}
					if currentError < bestError {
						bestError = currentError
						bestSlice = slice
						bestLMS.assign(lms)
					}
				}
				self.lMSes[c].assign(bestLMS)
				bestSlice <<= Int64((20 - sliceSamples) * 3)
				lastScaleFactors[c] = UInt8(bestSlice >> 60)
				if !writeLong(bestSlice) {
					return false
				}
			}
		}
		return true
	}

	private static let writeFrameReciprocals = [Int]([ 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 ])

	private static let writeFrameQuantTab = [UInt8]([ 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6,
		6 ])
}

/// Decoder of the "Quite OK Audio" format.
public class QOADecoder : QOABase
{

	/// Reads a byte from the stream.
	/// Returns the unsigned byte value or -1 on EOF.
	open func readByte() -> Int
	{
		preconditionFailure("Abstract method called")
	}

	/// Seeks the stream to the given position.
	/// - parameter position File offset in bytes.
	open func seekToByte(_ position : Int)
	{
		preconditionFailure("Abstract method called")
	}

	private var buffer : Int = 0

	private var bufferBits : Int = 0

	private func readBits(_ bits : Int) -> Int
	{
		while self.bufferBits < bits {
			let b : Int = readByte()
			if b < 0 {
				return -1
			}
			self.buffer = self.buffer << 8 | b
			self.bufferBits += 8
		}
		self.bufferBits -= bits
		let result : Int = self.buffer >> self.bufferBits
		self.buffer &= 1 << self.bufferBits - 1
		return result
	}

	private var totalSamples : Int = 0

	private var positionSamples : Int = 0

	/// Reads the file header.
	/// Returns `true` if the header is valid.
	public func readHeader() -> Bool
	{
		if readByte() != 113 || readByte() != 111 || readByte() != 97 || readByte() != 102 {
			return false
		}
		self.buffer = 0
		self.bufferBits = self.buffer
		self.totalSamples = readBits(32)
		if self.totalSamples <= 0 {
			return false
		}
		self.frameHeader = readBits(32)
		if self.frameHeader <= 0 {
			return false
		}
		self.positionSamples = 0
		let channels : Int = getChannels()
		return channels > 0 && channels <= 8 && getSampleRate() > 0
	}

	/// Returns the file length in samples per channel.
	public func getTotalSamples() -> Int
	{
		return self.totalSamples
	}

	private func getMaxFrameBytes() -> Int
	{
		return 8 + getChannels() * 2064
	}

	private func readLMS(_ result : ArrayRef<Int>) -> Bool
	{
		for i in 0..<4 {
			let hi : Int = readByte()
			if hi < 0 {
				return false
			}
			let lo : Int = readByte()
			if lo < 0 {
				return false
			}
			result[i] = (hi ^ 128 - 128) << 8 | lo
		}
		return true
	}

	/// Reads and decodes a frame.
	/// Returns the number of samples per channel.
	/// - parameter samples PCM samples.
	public func readFrame(_ samples : ArrayRef<Int16>) -> Int
	{
		if self.positionSamples > 0 && readBits(32) != self.frameHeader {
			return -1
		}
		let samplesCount : Int = readBits(16)
		if samplesCount <= 0 || samplesCount > 5120 || samplesCount > self.totalSamples - self.positionSamples {
			return -1
		}
		let channels : Int = getChannels()
		let slices : Int = (samplesCount + 19) / 20
		if readBits(16) != 8 + channels * (16 + slices * 8) {
			return -1
		}
		let lmses = ArrayRef<LMS>(factory: LMS.init, count: 8)
		for c in 0..<channels {
			if !readLMS(lmses[c].history) || !readLMS(lmses[c].weights) {
				return -1
			}
		}
		for sampleIndex in stride(from: 0, to: samplesCount, by: 20) {
			for c in 0..<channels {
				var scaleFactor : Int = readBits(4)
				if scaleFactor < 0 {
					return -1
				}
				scaleFactor = Int(QOADecoder.scaleFactors[scaleFactor])
				var sampleOffset : Int = sampleIndex * channels + c
				for s in 0..<20 {
					let quantized : Int = readBits(3)
					if quantized < 0 {
						return -1
					}
					if sampleIndex + s >= samplesCount {
						continue
					}
					let dequantized : Int = QOADecoder.dequantize(quantized, scaleFactor)
					let reconstructed : Int = QOADecoder.clamp(lmses[c].predict() + dequantized, -32768, 32767)
					lmses[c].update(reconstructed, dequantized)
					samples[sampleOffset] = Int16(reconstructed)
					sampleOffset += channels
				}
			}
		}
		self.positionSamples += samplesCount
		return samplesCount
	}

	/// Seeks to the given time offset.
	/// Requires the input stream to be seekable with `SeekToByte`.
	/// - parameter position Position from the beginning of the file.
	public func seekToSample(_ position : Int)
	{
		let frame : Int = position / 5120
		seekToByte(frame == 0 ? 12 : 8 + frame * getMaxFrameBytes())
		self.positionSamples = frame * 5120
	}

	/// Returns `true` if all frames have been read.
	public func isEnd() -> Bool
	{
		return self.positionSamples >= self.totalSamples
	}
}

public class ArrayRef<T> : Sequence
{
	var array : [T]

	init(_ array : [T])
	{
		self.array = array
	}

	init(repeating: T, count: Int)
	{
		self.array = [T](repeating: repeating, count: count)
	}

	init(factory: () -> T, count: Int)
	{
		self.array = (1...count).map({_ in factory() })
	}

	subscript(index: Int) -> T
	{
		get
		{
			return array[index]
		}
		set(value)
		{
			array[index] = value
		}
	}
	subscript(bounds: Range<Int>) -> ArraySlice<T>
	{
		get
		{
			return array[bounds]
		}
		set(value)
		{
			array[bounds] = value
		}
	}

	func fill(_ value: T)
	{
		array = [T](repeating: value, count: array.count)
	}

	func fill(_ value: T, _ startIndex : Int, _ count : Int)
	{
		array[startIndex ..< startIndex + count] = ArraySlice(repeating: value, count: count)
	}

	public func makeIterator() -> IndexingIterator<Array<T>>
	{
		return array.makeIterator()
	}
}
