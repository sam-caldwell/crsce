package types

import "testing"

func TestBit_IsSet(t *testing.T) {
	t.Run("test IsSet with clear", func(t *testing.T) {
		bit := Clear
		if bit.IsSet() {
			t.Fatal("bit.IsSet() expected true but got false")
		}
	})
	t.Run("test IsSet with set", func(t *testing.T) {
		bit := Set
		if !bit.IsSet() {
			t.Fatal("bit.IsSet() expected true but got false")
		}
	})
}
