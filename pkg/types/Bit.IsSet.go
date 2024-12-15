package types

// IsSet - return true if bit is set
func (b *Bit) IsSet() bool {
	return *b == Set
}
