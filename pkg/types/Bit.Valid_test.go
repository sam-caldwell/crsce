package types

import "testing"

func TestBit_Valid(t *testing.T) {
	t.Run("test valid Set", func(t *testing.T) {
		bit := Set
		if !bit.Valid() {
			t.Fatalf("Bit is not valid")
		}
	})
	t.Run("test valid Clear", func(t *testing.T) {
		bit := Clear
		if !bit.Valid() {
			t.Fatalf("Bit is not valid")
		}
	})
	t.Run("test with invalid value", func(t *testing.T) {
		bit := Bit(uint(Set) + 1) // Create an invalid Bit value
		if bit.Valid() {
			t.Fatal("Expected Valid to fail on invalid value")
		}
	})
}
