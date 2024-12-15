package types

import "testing"

func TestBit_IsClear(t *testing.T) {
	t.Run("test IsClear with clear", func(t *testing.T) {
		bit := Clear
		if !bit.IsClear() {
			t.Fatal("bit.IsClear() expected true but got false")
		}
	})
	t.Run("test IsClear with set", func(t *testing.T) {
		bit := Set
		if bit.IsClear() {
			t.Fatal("bit.IsClear() expected false but got true")
		}
	})
}
