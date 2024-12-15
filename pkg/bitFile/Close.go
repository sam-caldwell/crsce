package bitFile

// Close - close the bitFile
func (b *BitFile) Close() {
	if b.fh != nil {
		_ = b.fh.Close()
	}
}
