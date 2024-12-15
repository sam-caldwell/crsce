package csm

import (
	"crsce/pkg/types"
	"fmt"
)

// Set - set the CSM matrix value
func (m *Lockable) Set(r, c types.MatrixPosition, value types.Bit) (err error) {

	if err := boundsCheck(m, r, c); err != nil {

		return err

	}

	if value.Valid() {

		m.data[r][c].value = value

		return nil

	}

	return fmt.Errorf("value not valid")

}