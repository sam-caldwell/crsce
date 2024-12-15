package csm

import (
	"crsce/pkg/types"
	"fmt"
)

// Set - sets the data value of a CSM element
func (m *Simple) Set(r, c types.MatrixPosition, value types.Bit) error {

	if err := boundsCheck(m, r, c); err != nil {

		return err

	}

	if value.Valid() {

		m.data[r][c].value = value

		return nil

	}

	return fmt.Errorf("value not valid")

}
