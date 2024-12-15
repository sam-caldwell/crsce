package csm

import "crsce/pkg/types"

// resize - a method for resizing the Lockable CSM (used by New())
func (m *Lockable) resize(size types.MatrixPosition) error {

	m.data = make([][]LockingElement, size)

	for i := range m.data {

		m.data[i] = make([]LockingElement, size)

	}

	return nil

}
