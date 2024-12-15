package csm

// size - return the matrix size (s)
func (m *Lockable) size() uint {
	return uint(len(m.data))
}
