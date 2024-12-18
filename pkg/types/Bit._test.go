package types

import (
	"reflect"
	"testing"
)

func TestBitEnum(t *testing.T) {
	t.Run("happy path", func(t *testing.T) {
		var b Bit
		b = Clear
		if uint(b) != 0 {
			t.Errorf("BitEnum expect 0 but got %d", uint(b))
		}
		b = Set
		if uint(b) != 1 {
			t.Errorf("BitEnum expect 1 but got %d", uint(b))
		}
	})
	t.Run("verify underlying type", func(t *testing.T) {
		var b Bit
		expectedType := reflect.TypeOf(b).Kind()
		actualType := reflect.TypeOf(b).Kind()
		if actualType != expectedType {
			t.Fatalf("expected %v got %v", expectedType, actualType)
		}
	})
}
