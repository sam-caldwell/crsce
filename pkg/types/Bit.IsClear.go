package types

// IsClear - return true if bit is clear
func (b *Bit) IsClear() bool {
	return *b == Clear
}
