package hashMatrix

import (
	"crypto/sha256"
	"io"
	"os"
	"testing"
)

func TestMatrix_Serialize(t *testing.T) {
	const testFileName = "/tmp/crsce.matrix.test.file.txt"
	var (
		err      error
		testFile *os.File
		matrix   = Matrix{
			buffer:         make([]byte, 10),
			bufferPosition: 0,
			bitPosition:    0,
			size:           10,
			hash:           make([]Hash, 2),
		}
		expected = []Hash{
			sha256.Sum256([]byte("row0")),
			sha256.Sum256([]byte("row1")),
		}
	)

	t.Run("Initialize Matrix with two hashes and serialize", func(t *testing.T) {
		matrix.hash[0] = expected[0]
		matrix.hash[1] = expected[1]

		defer func() {
			if testFile != nil {
				_ = testFile.Close()
			}
			_ = os.Remove(testFileName)
		}()

		// Open the file for writing with appropriate flags
		testFile, err = os.OpenFile(testFileName, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0666)
		if err != nil {
			t.Fatalf("open file (create) error: %v", err)
		}

		// Serialize the matrix to the file
		if err := matrix.Serialize(testFile); err != nil {
			t.Fatal(err)
		}

		if err = testFile.Close(); err != nil {
			t.Fatal(err)
		}

		// Reopen the file for reading
		testFile, err = os.OpenFile(testFileName, os.O_RDONLY, 0666)
		if err != nil {
			t.Fatalf("open file (read) error: %v", err)
		}

		raw := make([]byte, 64)
		n, err := testFile.Read(raw)
		if err != nil && err != io.EOF {
			t.Fatalf("read failed (n=%d): %v", n, err)
		}

		if n != 64 {
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
