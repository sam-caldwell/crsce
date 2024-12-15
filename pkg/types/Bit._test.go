package types

import "testing"

func TestBitEnum(t *testing.T) {
	var b Bit
	b = Clear
	if uint(b) != 0 {
		t.Errorf("BitEnum expect 0 but got %d", uint(b))
	}
	b = Set
	if uint(b) != 1 {
		t.Errorf("BitEnum expect 1 but got %d", uint(b))
	}
}
