package types

import "testing"

func TestBit_FromByte(t *testing.T) {
	tests := []struct {
		byteValue byte
		bitPos    uint8
		expected  Bit
	}{
		{0b10101100, 0, Set},
		{0b10101100, 1, Clear},
		{0b10101100, 2, Set},
		{0b10101100, 3, Clear},
		{0b10101100, 4, Set},
		{0b10101100, 5, Set},
		{0b10101100, 6, Clear},
		{0b10101100, 7, Clear},
	}

	var result Bit
	for index, test := range tests {
		if err := result.FromByte(test.byteValue, test.bitPos); err != nil {
			t.Fatalf("error: %v", err)
		}
		if result != test.expected {
			t.Fatalf("#%d GetBit(%08b, %d) = %v; want %v",
				index, test.byteValue, test.bitPos, result, test.expected)
		}
	}
}
