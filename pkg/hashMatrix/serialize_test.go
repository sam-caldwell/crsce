package hashMatrix

import (
	"bytes"
	"crypto/sha256"
	"testing"
)

func TestMatrix_Serialize(t *testing.T) {
	const testFileName = "/tmp/crsce.matrix.test.file.txt"
	var (
		matrix = Matrix{
			buffer:         make([]byte, 10),
			bufferPosition: 0,
			bitPosition:    0,
			size:           10,
			hash:           make([]Hash, 2),
		}
		expected = []Hash{
			sha256.Sum256([]byte("row0")),
			sha256.Sum256([]byte("row1")),
			sha256.Sum256([]byte("Woohoo!")),
		}
	)

	t.Run("Initialize Matrix with two hashes and serialize", func(t *testing.T) {
		var buffer bytes.Buffer

		matrix.hash[0] = expected[0]
		matrix.hash[1] = expected[1]

		// Serialize the matrix to the file
		if err := matrix.Serialize(&buffer); err != nil {
			t.Fatal(err)
		}
		raw := buffer.Bytes()

		if n := len(raw); n != 64 {
			t.Fatalf("read raw data: expected 64 bytes, got %d", n)
		}

		var actual [2]Hash
		copy(actual[0][:], raw[0:32])
		copy(actual[1][:], raw[32:64])

		for i := 0; i < 2; i++ {
			if !actual[i].Equal(expected[i]) {
				t.Fatalf("Hash %d mismatch: Expected %v, Actual %v", i, expected[i], actual[i])
			}
		}
	})
}
