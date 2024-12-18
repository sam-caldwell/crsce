package hashMatrix

import (
	"crsce/pkg/types"
	"crypto/sha256"
)

// CaptureHash - Capture a hash of the hashMatrix Buffer
func (m *Matrix) CaptureHash(r types.MatrixPosition) {
	m.hash[r] = sha256.Sum256(m.buffer)
	m.bufferPosition, m.bitPosition = 0, 0
	m.buffer = make([]byte, m.size/8)
}
