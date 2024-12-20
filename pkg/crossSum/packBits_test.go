package crossSum

import (
	"fmt"
	"testing"
)

func TestPackBits(t *testing.T) {
	t.Run("Simple test: one sum (uint16) packed at various settings", func(t *testing.T) {
		tests := []struct {
			sum         uint16
			width       uint8
			bufferWidth uint8
			expected    uint16
			reason      string
		}{
			{sum: 0b0000000000000000, width: 0, bufferWidth: 1, expected: 0x0000, reason: "0 bits extracted"},
			{sum: 0b0000000000000001, width: 1, bufferWidth: 1, expected: 0x0001, reason: "1 bits extracted"},
			{sum: 0b0000000000000011, width: 2, bufferWidth: 1, expected: 0x0003, reason: "3 bits extracted"},
			{sum: 0b0000000000000111, width: 3, bufferWidth: 1, expected: 0x0007, reason: "7 bits extracted"},
			{sum: 0b0000000000001111, width: 4, bufferWidth: 1, expected: 0x000F, reason: "F bits extracted"},
			{sum: 0x5555, width: 8, bufferWidth: 1, expected: 0x0055, reason: "0x5555 8 bits extracted"},
			{sum: 0x5555, width: 16, bufferWidth: 2, expected: 0x5555, reason: "0x5555 16 bits extracted"},
			{sum: 0xAAAA, width: 16, bufferWidth: 2, expected: 0xAAAA, reason: "0xAAAA 16 bits extracted"},
			{sum: 0xFFFF, width: 16, bufferWidth: 2, expected: 0xFFFF, reason: "0xFFFF 16 bits extracted"},
		}

		for index, tt := range tests {
			var bitStack uint16
			t.Run(fmt.Sprintf("%d extract bits", index), func(t *testing.T) {
				bitStack = extractBits(tt.sum, tt.width)
			})
			t.Run(fmt.Sprintf("%d pack bits", index), func(t *testing.T) {
				var (
					bitPosition    uint8
					buffer         []byte = make([]byte, tt.bufferWidth)
					bufferPosition uint
					resultWidth    uint
				)

				packBits(&buffer, &bitPosition, &bufferPosition, &resultWidth, bitStack, tt.width)

			})
		}
	})
}
