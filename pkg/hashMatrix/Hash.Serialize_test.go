package hashMatrix

import (
	"bytes"
	"crypto/sha256"
	"testing"
)

func TestHash_Serialize(t *testing.T) {
	hash := Hash(sha256.Sum256([]byte("test")))

	expected := hash[:]
	actual := hash.Serialize()

	if !bytes.Equal(actual, expected) {
		t.Fatalf("expected:%v, actual:%v", expected, actual)
	}
}
