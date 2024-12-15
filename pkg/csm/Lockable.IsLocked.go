package csm

import "crsce/pkg/types"

// IsLocked - return the lock state for a given element
func (m *Lockable) IsLocked(r, c types.MatrixPosition) (bool, error) {

	if err := boundsCheck(m, r, c); err != nil {

		return true, err // Return true on error to fail-safe

	}

	return m.data[r][c].lock, nil

}
