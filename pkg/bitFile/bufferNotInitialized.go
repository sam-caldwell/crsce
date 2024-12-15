package bitFile

// bufferNotInitialized - return true if the buffer is not initialized
func (b *BitFile) bufferNotInitialized() bool {

	return b.buffer == nil

}
