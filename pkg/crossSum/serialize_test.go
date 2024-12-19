package crossSum

import (
	"bytes"
	"testing"
)

func TestMatrixSerialize(t *testing.T) {
	// Define test cases
	tests := []struct {
		name      string
		matrix    Matrix
		bitWidth  uint16
		expected  []byte
		expectErr bool
	}{
		{
			name: "Basic serialization",
			matrix: Matrix{
				0b1010000000000000, // 10100
				0b1010000000000000, // 10100
				0b1110000000000000, // 11100
			},
			bitWidth: 5,
			expected: []byte{
				0b10100101, // packed 10100 and the first three bits 101,
				0b00111000, // packed last two bits 00 and 11100 with trailing 0.
			},
		},
		{
			name: "Basic serialization",
			matrix: Matrix{
				0b1111111111111111, // 10100
				0b1010000000000001, // 10100
				0b1110000000000001, // 11100
			},
			bitWidth: 16,
			expected: []byte{
				0b11111111,
				0b11111111,
				0b10100000,
				0b00000001,
				0b11100000,
				0b00000001,
			},
		},
		//{
		//	name: "width (b) exceeds uint16",
		//	matrix: Matrix{
		//		0b0000010100000000,
		//		0b0010000000000000},
		//	bitWidth:  17,
		//	expectErr: true,
		//},
		//{
		//	name:     "8-bit serialization",
		//	matrix:   Matrix{0b11111111, 0b10000000, 0b01000000},
		//	bitWidth: 8,                    // Each element fits in 8 bits
		//	expected: []byte{255, 128, 64}, // Direct representation
		//},
		//{
		//	name:     "9-bit serialization",
		//	matrix:   Matrix{0b0000000111111111, 0b0000000100000000}, // Each element fits in 9 bits
		//	bitWidth: 9,
		//	expected: []byte{0x01, 0xFF, 0x01, 0x00}, // Packed: 511=0x01,0xFF, 256=0x01,0x00
		//},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var buffer bytes.Buffer // Use in-memory buffer instead of a file

			// Call Serialize
			err := tt.matrix.Serialize(&buffer, tt.bitWidth)

			// Check for expected error
			if tt.expectErr {
				if err == nil {
					t.Fatalf("expected an error but got none")
				}
				return // No further checks if error was expected
			}

			// Check for unexpected errors
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}

			// Verify the serialized output
			output := buffer.Bytes()
			if !bytes.Equal(output, tt.expected) {
				t.Errorf("unexpected output (%s):\n"+
					" got %08b\n"+
					"want %08b",
					tt.name, output, tt.expected)
			}
		})
	}
}
