package hashMatrix

import (
	"crsce/pkg/types"
)

// Matrix - a matrix of hashes
type Matrix struct {
	buffer         []byte
	bufferPosition uint
	bitPosition    uint8
	size           types.MatrixPosition
	hash           []Hash
}
