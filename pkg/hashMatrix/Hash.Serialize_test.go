package hashMatrix

import (
	"bytes"
	"crypto/sha256"
	"testing"
)

func TestHash_Serialize(t *testing.T) {
	hash := []Hash{
		sha256.Sum256([]byte("test1")),
		sha256.Sum256([]byte("test2")),
		sha256.Sum256([]byte("test3")),
	}

	var actual []byte
	var expected []byte
	for _, h := range hash {
		expected = append(expected, h[:]...)
		actual = append(actual, h.Serialize()...)
	}
	if !bytes.Equal(actual, expected) {
		t.Fatalf("expected:%v, actual:%v", expected, actual)
	}
}
