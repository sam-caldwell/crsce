package common

import (
	"crsce/pkg/types"
	"testing"
)

func TestGetBit(t *testing.T) {
	tests := []struct {
		byteValue byte
		bitPos    uint8
		expected  types.Bit
	}{
		{0b10101100, 0, types.Set},
		{0b10101100, 1, types.Clear},
		{0b10101100, 2, types.Set},
		{0b10101100, 3, types.Clear},
		{0b10101100, 4, types.Set},
		{0b10101100, 5, types.Set},
		{0b10101100, 6, types.Clear},
		{0b10101100, 7, types.Clear},
	}

	for index, test := range tests {
		result := GetBit(test.byteValue, test.bitPos)
		if result != test.expected {
			t.Fatalf("#%d GetBit(%08b, %d) = %v; want %v",
				index, test.byteValue, test.bitPos, result, test.expected)
		}
	}
}
