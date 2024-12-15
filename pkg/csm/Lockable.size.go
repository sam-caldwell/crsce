package csm

import "crsce/pkg/types"

// size - return the matrix size (s)
func (m *Lockable) size() types.MatrixPosition {
	return types.MatrixPosition(len(m.data))
}
