package hashMatrix

import (
	"crsce/pkg/types"
	"fmt"
)

// Push - push a given bit to the position (r,c)
func (m *Matrix) Push(r, c types.MatrixPosition, bit types.Bit) {

	// push the given bit to the current buffer state
	if boundary := m.size * 8; r > boundary || c > boundary {
		panic(fmt.Sprintf("coordinates outside CSM matrix (%d,%d) (size: %d)", r, c, m.size))
	}
	if !bit.Valid() {
		panic(fmt.Sprintf("invalid bit input(%d)", bit))
	}
	// reverse the bit position.
	//
	// typically the bit position is from right to left
	//     7 6 5 4 | 3 2 1 0
	//     0 0 0 0 | 0 0 0 0
	//
	// But, we need the other direction
	//     0 1 2 3 | 4 5 6 7
	//     0 0 0 0 | 0 0 0 0
	//
	reversedBitPos := 7 - m.bitPosition
	// Extract the bit from the reversed position
	m.buffer[m.bufferPosition] = m.buffer[m.bufferPosition] | (uint8(bit) << reversedBitPos)
	m.bitPosition++
	if m.bitPosition == 8 {
		m.bitPosition = 0
		m.bufferPosition++
	}
}
