package csm

import (
	"crsce/pkg/types"
	"fmt"
)

func (m *Simple) get(r uint, c uint) (value types.Bit, err error) {
	if sz := len(m.data); r >= sz || c >= sz {
		return fmt.Errorf("bounds check failed (r:%d, c:%d, sz:%d)", r, c, sz)
	}
}
