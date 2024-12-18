package hashMatrix

import (
	"crsce/pkg/types"
	"sync"
)

// Matrix - a matrix of hashes
type Matrix struct {
	lock           sync.RWMutex
	buffer         []byte
	bufferPosition uint
	bitPosition    uint8
	size           types.MatrixPosition
	hash           []Hash
}
