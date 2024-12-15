package bitFile

// initializeBuffer - initialize the BitFile state and its buffer memory
func (b *BitFile) initializeBuffer() {

	b.bufferPosition = 0

	b.bitPos = 0

	b.buffer = make([]byte, bufferSize)

}
