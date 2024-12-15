package hashMatrix

import "crsce/pkg/types"

// appendBit - given a bit value, store the bit to the current buffer byte.
func (m *Matrix) appendBit(bit types.Bit) {
	m.buffer[m.bufferPosition] = m.buffer[m.bufferPosition] | (uint8(bit) << m.bitPosition)
}
