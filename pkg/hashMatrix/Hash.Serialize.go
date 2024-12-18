package hashMatrix

// Serialize - serialize the Hash as []byte
func (lhs *Hash) Serialize() []byte {
	return (*lhs)[:]
}
