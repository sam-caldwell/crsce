package hashMatrix

import "testing"

func TestHashMatrix(t *testing.T) {
	_ = Matrix{
		buffer:         make([]byte, 10),
		bufferPosition: 1048576,
		bitPosition:    255,
		size:           1048576,
		hash:           make([]Hash, 512),
	}
}
