package crossSum

// New - initialize the Matrix with the given size
func New(size uint) Matrix {
	if size == 0 {
		panic("bounds check.  size must be greater than 0")
	}
	return make(Matrix, size)
}
