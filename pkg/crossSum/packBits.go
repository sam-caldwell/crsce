package crossSum

import "github.com/sam-caldwell/ansi"

// packBits - Pack bits from a given bitStack to only consume the bits in width
func packBits(buffer *[]byte, bitPosition, bufferPosition, resultWidth *uint, bitStack uint16, width uint8) {
	popBit := func() (bit byte) {
		defer func() { bitStack >>= 1 }() // Shift bit right
		return byte(bitStack & 1)         // Pop the bit
	}
	// Pop from the bitStack (flipping order back) and store them in our buffer as the packed product
	for i := uint8(0); i < width; i++ {
		// Shift the existing bits to make room for the new bit
		(*buffer)[*bufferPosition] <<= 1
		// OR the new bit into the buffer
		(*buffer)[*bufferPosition] |= popBit()

		// Increment bitPosition
		*bitPosition++

		// Check if we've filled a byte
		if (*bitPosition) == 8 {
			ansi.Printf("bitPosition (%d) reached 8. Incrementing bufferPosition to %d", *bitPosition, *bufferPosition)
			// Move to the next byte in the buffer
			*bufferPosition++
			// Reset bitPosition
			*bitPosition = 0
			// Increment resultWidth by 8 bits
			*resultWidth += 8
		} else {
			ansi.Printf("bitPosition %d", *bitPosition)
		}
		ansi.LF()
	}
}
