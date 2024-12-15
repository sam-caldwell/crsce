package bitFile

// loadBufferIfConsumed - load data into the buffer if it is empty
func (b *BitFile) loadBufferIfConsumed() (err error) {

	if b.bufferExhausted() || b.bufferNotInitialized() {

		b.initializeBuffer()

		if _, err = b.fh.Read(b.buffer); err != nil {
			return err
		}

	}

	return nil

}
