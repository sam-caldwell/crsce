package types

import "fmt"

// FromByte - given a byte value and desired bit position, set the Bit to the corresponding bit value
func (b *Bit) FromByte(byteValue byte, bitPosition uint8) error {
	if bitPosition > 7 {
		return fmt.Errorf("bit position out of range")
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
	reversedBitPos := 7 - bitPosition

	// Extract the bit from the reversed position
	*b = Bit((byteValue & (1 << reversedBitPos)) >> reversedBitPos)
	return nil
}
