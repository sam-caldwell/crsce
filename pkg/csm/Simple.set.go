package csm

import (
	"crsce/pkg/types"
	"fmt"
)

// set - sets the data value of a CSM element
func (m *Simple) set(r uint, c uint, value types.Bit) error {
	if sz := uint(len(m.data)); r >= sz || c >= sz {
		return fmt.Errorf("bounds check failed (r:%d, c:%d, sz:%d)", r, c, sz)
	}
	if value.Valid() {
		m.data[r][c].value = value
		return nil
	}
	return fmt.Errorf("value not valid")
}
