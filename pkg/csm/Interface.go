package csm

import "crsce/pkg/types"

// Interface - an abstract type for working with CSM matrices
type Interface interface {
	resize(size int) error

	set(r int, c int, value types.Bit) error

	size() uint
}
