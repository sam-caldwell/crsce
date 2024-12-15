package types

type Bit uint8

const (
	Clear Bit = iota
	Set
)

// String - return the string value for the bit
func (b Bit) String() string {
	switch b {
	case Clear:
		return "clear"
	case Set:
		return "set"
	default:
		panic("invalid bit value")
	}
}

// IsClear - return true if bit is clear
func (b Bit) IsClear() bool {
	return b == Clear
}

// IsSet - return true if bit is set
func (b Bit) IsSet() bool {
	return b == Set
}

// Valid - make sure our bit value is valid (nothing magical...)
func (b Bit) Valid() bool {
	return b == Clear || b == Set
}
