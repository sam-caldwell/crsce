package hashMatrix

import "crsce/pkg/types"

// appendBit - given a bit value, store the bit to the current buffer byte.
func (h *Matrix) appendBit(bit types.Bit) {
	h.buffer[h.bufferPosition] = h.buffer[h.bufferPosition] | (uint8(bit) << h.bitPosition)
}
