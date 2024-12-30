package crossSum

import (
	"bytes"
	"fmt"
	"testing"
)

func TestPackBits(t *testing.T) {
	t.Run("Simple test: one sum (uint16) packed at various settings", func(t *testing.T) {
		tests := []struct {
			sum         uint16
			width       uint8
			bufferWidth uint8
			expected    []byte
			reason      string
		}{
			{sum: 0b0000000000000000, width: 0, bufferWidth: 1, expected: []byte{0x00}, reason: "0 bits extracted"},
			{sum: 0b0000000000000001, width: 1, bufferWidth: 1, expected: []byte{0x01}, reason: "1 bits extracted"},
			{sum: 0b0000000000000011, width: 2, bufferWidth: 1, expected: []byte{0x03}, reason: "3 bits extracted"},
			{sum: 0b0000000000000111, width: 3, bufferWidth: 1, expected: []byte{0x07}, reason: "7 bits extracted"},
			{sum: 0b0000000000001111, width: 4, bufferWidth: 1, expected: []byte{0x0F}, reason: "F bits extracted"},
			{sum: 0x5555, width: 8, bufferWidth: 1, expected: []byte{0x55}, reason: "0x5555 8 bits extracted"},
			{sum: 0x0055, width: 16, bufferWidth: 2, expected: []byte{0x00, 0x55}, reason: "0x0055 16 bits extracted"},
			{sum: 0x5500, width: 16, bufferWidth: 2, expected: []byte{0x55, 0x00}, reason: "0x5500 16 bits extracted"},
			{sum: 0x5555, width: 16, bufferWidth: 2, expected: []byte{0x55, 0x55}, reason: "0x0055 16 bits extracted"},
			{sum: 0xAAAA, width: 16, bufferWidth: 2, expected: []byte{0xAA, 0xAA}, reason: "0xAAAA 16 bits extracted"},
			{sum: 0xFFFF, width: 16, bufferWidth: 2, expected: []byte{0xFF, 0xFF}, reason: "0xFFFF 16 bits extracted"},
		}

		for index, tt := range tests {
			var bitStack uint16
			t.Run(fmt.Sprintf("%d extract bits", index), func(t *testing.T) {
				bitStack = extractBits(tt.sum, tt.width)
			})
			t.Run(fmt.Sprintf("%d pack bits", index), func(t *testing.T) {
				var (
					bitPosition    uint
					buffer         []byte = make([]byte, tt.bufferWidth)
					bufferPosition uint
					resultWidth    uint
				)
				packBits(&buffer, &bitPosition, &bufferPosition, &resultWidth, bitStack, tt.width)
				if !bytes.Equal(buffer, tt.expected) {
					t.Fatalf("%d outcome unexpected\n"+
						"got: %v\n"+
						"want: %v\n"+
						"reaon: %s",
						index, buffer, tt.expected, tt.reason)
				}
			})
		}
	})
	t.Run("Complex test: many sums ([]uint16) packed at various settings", func(t *testing.T) {
		tests := []struct {
			sum         []uint16
			width       uint8
			bufferWidth uint8
			expected    []byte
			reason      string
		}{
			{sum: []uint16{0x00}, width: 0, bufferWidth: 1, expected: []byte{0x00}, reason: "1 sum, 0 bits extracted"},
			{sum: []uint16{0x01}, width: 1, bufferWidth: 1, expected: []byte{0x01}, reason: "1 sum, 1 bits extracted"},
			{sum: []uint16{0x03}, width: 2, bufferWidth: 1, expected: []byte{0x03}, reason: "1 sum, 3 bits extracted"},
			{sum: []uint16{0x07}, width: 3, bufferWidth: 1, expected: []byte{0x07}, reason: "1 sum, 7 bits extracted"},
			{sum: []uint16{0x0F}, width: 4, bufferWidth: 1, expected: []byte{0x0F}, reason: "1 sum, F bits extracted"},
			{sum: []uint16{0x5555}, width: 8, bufferWidth: 1, expected: []byte{0x55}, reason: "1 sum, 0x5555 8 bits extracted"},
			{sum: []uint16{0x0055}, width: 16, bufferWidth: 2, expected: []byte{0x00, 0x55}, reason: "1 sum, 0x0055 16 bits extracted"},
			{sum: []uint16{0x5500}, width: 16, bufferWidth: 2, expected: []byte{0x55, 0x00}, reason: "1 sum, 0x5500 16 bits extracted"},
			{sum: []uint16{0x5555}, width: 16, bufferWidth: 2, expected: []byte{0x55, 0x55}, reason: "1 sum, 0x0055 16 bits extracted"},
			{sum: []uint16{0xAAAA}, width: 16, bufferWidth: 2, expected: []byte{0xAA, 0xAA}, reason: "1 sum, 0xAAAA 16 bits extracted"},
			{sum: []uint16{0xFFFF}, width: 16, bufferWidth: 2, expected: []byte{0xFF, 0xFF}, reason: "1 sum, 0xFFFF 16 bits extracted"},

			{sum: []uint16{0x00, 0x00, 0x00, 0x00}, width: 1, bufferWidth: 1, expected: []byte{0x00}, reason: "pack 4 bits 0"},
			{sum: []uint16{0xFF, 0xFF, 0xFF, 0xFF}, width: 1, bufferWidth: 1, expected: []byte{0x0F}, reason: "pack 4 bits from 0xFF"},
			{sum: []uint16{0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, width: 1, bufferWidth: 1, expected: []byte{0x0F}, reason: "pack 4 bits from 0xFFFF"},
			{sum: []uint16{0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, width: 16, bufferWidth: 8, expected: []byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, reason: "pack 4x16 bits from 0xFFFF"},
			{sum: []uint16{0x5555, 0x5555, 0x5555, 0x5555}, width: 16, bufferWidth: 8, expected: []byte{0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, reason: "pack 4x16 bits from 0x5555"},
			{sum: []uint16{0x8000, 0x00, 0x00, 0x00}, width: 16, bufferWidth: 8, expected: []byte{0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, reason: "pack 4x16 bits from 0x80"},
			{sum: []uint16{0x0000, 0x00, 0x00, 0x01}, width: 16, bufferWidth: 8, expected: []byte{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, reason: "pack 4x16 bits from 0x01"},
			{sum: []uint16{0x0000, 0x00, 0x00, 0x01}, width: 1, bufferWidth: 1, expected: []byte{0x01}, reason: "pack 4x16 bits from 0x01 b1"},
			{sum: []uint16{0x03, 0x03, 0x03, 0x03}, width: 2, bufferWidth: 1, expected: []byte{0xFF}, reason: "pack 4x16 bits from 0x03 b2"},
		}

		for index, tt := range tests {
			t.Run(fmt.Sprintf("Test case %d: %s", index, tt.reason), func(t *testing.T) {
				var (
					bitPosition    uint
					buffer         []byte = make([]byte, tt.bufferWidth)
					bufferPosition uint
					resultWidth    uint
				)

				for _, sum := range tt.sum {
					bitStack := extractBits(sum, tt.width)
					packBits(&buffer, &bitPosition, &bufferPosition, &resultWidth, bitStack, tt.width)
				}

				if !bytes.Equal(buffer, tt.expected) {
					t.Fatalf("Test %d failed: %s\n"+
						"Got:    %v\n"+
						"Want:   %v",
						index, tt.reason, buffer, tt.expected)
				}
			})
		}
	})
}
