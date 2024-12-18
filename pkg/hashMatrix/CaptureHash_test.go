package hashMatrix

import (
	"bytes"
	"crypto/sha256"
	"testing"
)

func TestMatrix_CaptureHash(t *testing.T) {
	// isBufferCleared checks if a buffer is cleared (all zeroes)
	isBufferCleared := func(buffer []byte) bool {
		for _, b := range buffer {
			if b != 0 {
				return false
			}
		}
		return true
	}
	t.Run("empty buffer", func(t *testing.T) {
		for index := 0; index < 10; index++ {
			testData := []byte(generateRandomString(512 / 8))
			expected := sha256.Sum256(testData)
			m := Matrix{
				buffer:         testData,
				bufferPosition: 0,
				bitPosition:    0,
				size:           0,
				hash:           make([]Hash, 1),
			}
			m.CaptureHash(0)
			if !bytes.Equal(m.hash[0][:], expected[:]) {
				t.Fatalf("test (%d) hash mismatch\n"+
					"Expected %v\n"+
					"     got %v", index, expected, m.hash[0])
			}
			if !isBufferCleared(m.buffer) {
				t.Fatalf("test (%d) buffer not cleared", index)
			}
		}
	})
}
