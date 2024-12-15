package csm

import "crsce/pkg/types"

func (m *Lockable) Lock(r, c types.MatrixPosition) error {

	if err := boundsCheck(m, r, c); err != nil {

		return err

	}

	m.data[r][c].lock = true

	return nil

}
