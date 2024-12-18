package hashMatrix

import "testing"

func TestHashMatrix_New(t *testing.T) {
	m := New(512)
	if len(m.buffer) != 512/8 {
		t.Fatal("buffer not sized correctly")
	}
	if len(m.hash) != 512 {
		t.Fatal("hash not sized correctly")
	}
	if m.size != 512 {
		t.Fatal("size not stored correctly")
	}
}
