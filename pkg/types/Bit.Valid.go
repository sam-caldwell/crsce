package types

// Valid - make sure our bit value is valid (nothing magical...)
func (b *Bit) Valid() bool {
	return *b == Clear || *b == Set
}
