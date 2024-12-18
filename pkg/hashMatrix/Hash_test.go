package hashMatrix

import (
	"reflect"
	"testing"
)

func TestHash(t *testing.T) {
	// Expected type
	expectedType := reflect.TypeOf([32]byte{})

	// Actual type
	actualType := reflect.TypeOf(Hash{})

	// Verify the underlying type of Hash
	if actualType.Kind() != reflect.Array || actualType.Len() != 32 || actualType.Elem().Kind() != expectedType.Elem().Kind() {
		t.Errorf("Hash type mismatch: expected underlying type to be [32]byte, got %v", actualType)
	}

	// Verify assignability to [32]byte
	if !reflect.TypeOf(Hash{}).ConvertibleTo(expectedType) {
		t.Errorf("Hash type is not convertible to [32]byte")
	}
}
