package bitFile

func (b *BitFile) IncrementBitPosition() {
	b.bitPos++
	if b.bitPos > 7 {
		b.bufferPosition++
		b.bitPos = 0
	}
}
