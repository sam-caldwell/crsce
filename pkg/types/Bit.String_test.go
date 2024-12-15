package types

import "testing"

func TestBit_String(t *testing.T) {
	t.Run("test bit.String() with clear", func(t *testing.T) {
		bit := Clear
		if actual := bit.String(); actual != "clear" {
			t.Fatalf("expecting clear, but got %s", actual)
		}
	})
	t.Run("test bit.String() with set", func(t *testing.T) {
		bit := Set
		if actual := bit.String(); actual != "set" {
			t.Fatalf("expecting set, but got %s", actual)
		}
	})
	t.Run("test bit.String() with invalid", func(t *testing.T) {
		bit := Bit(uint(Set) + 1) // Create an invalid Bit value
		defer func() {
			if err := recover(); err != nil {
				// Assert that the panic is expected
				return
			}
			// If no panic occurs, the test should fail
			t.Fatal("Expected panic but none occurred")
		}()
		_ = bit.String() // This should trigger a panic
	})
}
