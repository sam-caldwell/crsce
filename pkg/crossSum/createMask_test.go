package crossSum

import (
	"fmt"
	"math"
	"testing"
)

func Test_createMask(t *testing.T) {
	tests := []struct {
		width    uint
		expected uint16
	}{
		{width: 0, expected: 0x0000},                  // No bits set
		{width: 1, expected: 0x0001},                  // Least significant bit set
		{width: 8, expected: 0x00FF},                  // First 8 bits set
		{width: 15, expected: 0x7FFF},                 // All bits except the MSB set
		{width: 16, expected: uint16(math.MaxUint16)}, // All bits set
		{width: 20, expected: uint16(math.MaxUint16)}, // Width > 16 clamps to all bits set
		{width: 32, expected: uint16(math.MaxUint16)}, // Width > 16 clamps to all bits set
		{width: 64, expected: uint16(math.MaxUint16)}, // Width > 16 clamps to all bits set
	}

	for _, test := range tests {
		t.Run(fmt.Sprintf("Width_%d", test.width), func(t *testing.T) {
			result := createMask(test.width)
			if result != test.expected {
				t.Errorf("createMask(%d) = %016b; want %016b", test.width, result, test.expected)
			}
		})
	}
}
