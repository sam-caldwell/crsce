package types

// String - return the string value for the bit
func (b *Bit) String() string {
	switch *b {
	case Clear:
		return "clear"
	case Set:
		return "set"
	default:
		panic("invalid bit value")
	}
}
