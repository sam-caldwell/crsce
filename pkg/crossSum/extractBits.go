package crossSum

// extractBits - push bits from sum to our bitStack (extracting what we want and flipping order)
func extractBits(sum uint16, width uint8) (bitStack uint16) {
	collectBit := func(value uint16, position uint8) {
		mask := uint16(1 << position)
		bit := (value & mask) >> position // Extract the bit at the position
		bitStack <<= 1                    // Shift the stack left
		bitStack |= bit                   // Add the extracted bit to the stack
	}

	for i := uint8(0); i < width; i++ {
		collectBit(sum, i)
	}

	return bitStack
}
