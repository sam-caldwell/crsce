package common

import "crsce/pkg/types"

// GetBit - return the bit at a given position of the input byte
func GetBit(currentByte byte, bitPos uint8) types.Bit {
	if bitPos > 7 {
		panic("bit position out of range")
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
	reversedBitPos := 7 - bitPos

	// Extract the bit from the reversed position
	return types.Bit((currentByte & (1 << reversedBitPos)) >> reversedBitPos)

}
