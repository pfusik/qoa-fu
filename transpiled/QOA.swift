// Generated automatically with "cito". Do not edit.

/// Least Mean Squares Filter.
class LMS
{

	fileprivate let history = ArrayRef<Int>(repeating: 0, count: 4)

	fileprivate let weights = ArrayRef<Int>(repeating: 0, count: 4)

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

/// Decoder of the "Quite OK Audio" format.
public class QOADecoder
{
	/// Constructs the decoder.
	/// The decoder can be used for several files, one after another.
	public init()
	{
	}

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

	private var expectedFrameHeader : Int = 0

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
		self.expectedFrameHeader = readBits(32)
		if self.expectedFrameHeader <= 0 {
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

	/// Maximum number of channels supported by the format.
	public static let maxChannels = 8

	/// Returns the number of audio channels.
	public func getChannels() -> Int
	{
		return self.expectedFrameHeader >> 24
	}

	/// Returns the sample rate in Hz.
	public func getSampleRate() -> Int
	{
		return self.expectedFrameHeader & 16777215
	}

	private static let sliceSamples = 20

	private static let maxFrameSlices = 256

	/// Maximum number of samples per frame.
	public static let maxFrameSamples = 5120

	private func getMaxFrameBytes() -> Int
	{
		return 8 + getChannels() * 2064
	}

	private static func clamp(_ value : Int, _ min : Int, _ max : Int) -> Int
	{
		return value < min ? min : value > max ? max : value
	}

	private func readLMS(_ result : ArrayRef<Int>?) -> Bool
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
			result![i] = (hi ^ 128 - 128) << 8 | lo
		}
		return true
	}

	/// Reads and decodes a frame.
	/// Returns the number of samples per channel.
	/// - parameter samples PCM samples.
	public func readFrame(_ samples : ArrayRef<Int16>?) -> Int
	{
		if self.positionSamples > 0 && readBits(32) != self.expectedFrameHeader {
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
				scaleFactor = Int(QOADecoder.readFrameScaleFactors[scaleFactor])
				var sampleOffset : Int = sampleIndex * channels + c
				for s in 0..<20 {
					let quantized : Int = readBits(3)
					if quantized < 0 {
						return -1
					}
					if sampleIndex + s >= samplesCount {
						continue
					}
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
					if quantized & 1 != 0 {
						dequantized = -dequantized
					}
					let reconstructed : Int = QOADecoder.clamp(lmses[c].predict() + dequantized, -32768, 32767)
					lmses[c].update(reconstructed, dequantized)
					samples![sampleOffset] = Int16(reconstructed)
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

	private static let readFrameScaleFactors = [UInt16]([ 1, 7, 21, 45, 84, 138, 211, 304, 421, 562, 731, 928, 1157, 1419, 1715, 2048 ])
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
