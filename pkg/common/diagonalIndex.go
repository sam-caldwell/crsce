package common

// Helper function to calculate diagonal index
func diagonalIndex(r, c, size int) int {
	return (r + c) % size
}
