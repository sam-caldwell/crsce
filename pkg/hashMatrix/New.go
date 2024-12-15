package hashMatrix

import "crsce/pkg/types"

// New - return an initialized Matrix
func New(size types.MatrixPosition) *Matrix {
	return &(Matrix{
		buffer: make([]byte, size/8),
		hash:   make([]Hash, size),
		size:   size,
	})
}
