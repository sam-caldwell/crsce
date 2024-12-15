package csm

import (
	"crsce/pkg/types"
	"fmt"
)

// set - set the CSM matrix value
func (m *Lockable) set(r uint, c uint, value types.Bit) error {
	if sz := len(m.data); r >= sz || c >= sz {
		return fmt.Errorf("bounds check failed (r:%d, c:%d, sz:%d)", r, c, sz)
	}
	if value.Valid() {
		m.data[r][c].value = value
		return nil
	}
	return fmt.Errorf("value not valid")
}
