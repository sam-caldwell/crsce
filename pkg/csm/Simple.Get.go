package csm

import (
	"crsce/pkg/types"
)

// Get - returns the given element for the CSM matrix
func (m *Simple) Get(r, c types.MatrixPosition) (value types.Bit, err error) {

	if err := boundsCheck(m, r, c); err != nil {

		return types.Clear, err

	}

	return m.data[r][c].value, nil

}
