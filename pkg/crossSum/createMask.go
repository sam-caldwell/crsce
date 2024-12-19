package crossSum

// createMask - create a 16-bit binary mask for a number of bits (width)
func createMask(width uint) uint16 {
	// Ensure width is within the range of uint16 (0 to 16 bits)
	if width > 16 {
		width = 16
	}
	return uint16((1 << width) - 1)
}
