package common

// Helper function to calculate anti-diagonal index
func antiDiagonalIndex(r, c, size int) int {
	return (size - r + c - 1) % size
}
