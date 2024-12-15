package hashMatrix

import (
	"crsce/pkg/types"
	"crypto/sha256"
)

// Push - push a given bit to the position (r,c)
func (m *Matrix) Push(r, c types.MatrixPosition, bit types.Bit) {

	if c >= m.size {
		//if buffer is full, store its hash and reset the buffer
		m.hash[r] = sha256.Sum256(m.buffer)
		r, c = 0, 0
		m.bufferPosition, m.bitPosition = 0, 0
		m.buffer = make([]byte, m.size/8)
	}

	// push the given bit to the current buffer state
	m.appendBit(bit)
}
