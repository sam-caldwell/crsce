package hashMatrix

import (
	"crsce/pkg/types"
	"crypto/sha256"
)

// Push - push a given bit to the position (r,c)
func (h *Matrix) Push(r, c types.MatrixPosition, bit types.Bit) {

	if c >= h.size {
		//if buffer is full, store its hash and reset the buffer
		h.hash[r] = sha256.Sum256(h.buffer)
		r, c = 0, 0
		h.bufferPosition, h.bitPosition = 0, 0
		h.buffer = make([]byte, h.size/8)
	}

	// push the given bit to the current buffer state
	h.appendBit(bit)
}
