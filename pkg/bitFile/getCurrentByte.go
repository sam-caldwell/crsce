package bitFile

// getCurrentByte - return the byte at the current buffer position
func (b *BitFile) getCurrentByte() byte {
	return b.buffer[b.bufferPosition]
}
