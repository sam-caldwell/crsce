package csm

import "crsce/pkg/types"

// Interface - an abstract type for working with CSM matrices
type Interface interface {
	resize(size types.MatrixPosition) error

	Set(r, c types.MatrixPosition, value types.Bit) error

	Get(r, c types.MatrixPosition) (types.Bit, error)

	size() types.MatrixPosition
}
