package bitFile

// bufferExhausted - return true if the buffer has been exhausted
func (b *BitFile) bufferExhausted() bool {
	return b.bufferPosition >= bufferSize
}
