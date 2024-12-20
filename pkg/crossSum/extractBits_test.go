package crossSum

import (
	"fmt"
	"testing"
)

func Test_extractBits(t *testing.T) {

	tests := []struct {
		sum      uint16
		width    uint8
		expected uint16
		reason   string
	}{
		{sum: 0b00000000, width: 0, expected: 0x0000, reason: "No bits extracted"},
		{sum: 0b00000000, width: 16, expected: 0x0000, reason: "16 clear bits"},
		{sum: 0b00000001, width: 1, expected: 0x0001, reason: "grab just one set bit from a byte"},
		{sum: 0x0001, width: 1, expected: 0x0001, reason: "grab just one set bit from a 16-bit word"},
		{sum: 0b00000011, width: 2, expected: 0x0003, reason: "given two set bits in a byte, extract the two bits"},
		{sum: 0x0003, width: 2, expected: 0x0003, reason: "given two set bits in a word, extract the two bits"},
		{sum: 0b01011011, width: 4, expected: 0x000D, reason: "extract only the first four bits of a byte"},
		{sum: 0b0101101101011011, width: 4, expected: 0x000D, reason: "extract only the first four bits of a word"},
		{sum: 0b10110011, width: 8, expected: 0xCD, reason: "extract only the eight bits of a byte"},
		{sum: 0b0000000010110011, width: 8, expected: 0xCD, reason: "extract only the eight bits of a word"},
		{sum: 0b11110000, width: 4, expected: 0x0000, reason: "extract the first four bits of a byte"},
		{sum: 0b11110000, width: 8, expected: 0x000F, reason: "extract the eight bits of a byte"},
		{sum: 0b0000000011110000, width: 4, expected: 0x0000, reason: "extract the four bits of a word"},
		{sum: 0b0000000011110000, width: 8, expected: 0x000F, reason: "extract the eight bits of a word"},
		{sum: 0xFFFF, width: 16, expected: 0xFFFF, reason: "all 16 bits set to 1"},
		{sum: 0b1111000011110000, width: 16, expected: 0b0000111100001111, reason: "alternating set byte"},
		{sum: 0x5555, width: 16, expected: 0xAAAA}, // test alternating 0
		{sum: 0xAAAA, width: 16, expected: 0x5555}, // test alternating 1
		{sum: 0x8000, width: 16, expected: 0x0001},
	}

	for index, tt := range tests {
		t.Run(fmt.Sprintf("sum=%016b,width=%d", tt.sum, tt.width), func(t *testing.T) {
			result := extractBits(tt.sum, tt.width)
			if result != tt.expected {
				t.Fatalf("Test %d: extractBits(%016b, %d) should reverse bit order as a stack\n"+
					"   got: %016b\n"+
					"  want: %016b\n"+
					"reason: %s",
					index, tt.sum, tt.width, result, tt.expected, tt.reason)
			}
		})
	}
}
