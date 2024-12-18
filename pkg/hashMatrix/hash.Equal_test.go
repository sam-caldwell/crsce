package hashMatrix

import (
	"bytes"
	"crypto/sha256"
	"testing"
)

func TestHashMatrix_Hash_Equal(t *testing.T) {
	t.Run("equal inputs", func(t *testing.T) {
		h := sha256.Sum256([]byte("row0"))
		lhs := h[:]
		rhs := h[:]
		if !bytes.Equal(lhs, rhs) {
			t.Fatalf("hashes do not match")
		}
	})
	t.Run("unequal inputs", func(t *testing.T) {
		h1 := sha256.Sum256([]byte("row0"))
		h2 := sha256.Sum256([]byte("row1"))
		lhs := h1[:]
		rhs := h2[:]
		if bytes.Equal(lhs, rhs) {
			t.Fatalf("hashes should not match")
		}
	})
}
