package compress

import (
	"crsce/pkg/common"
	"crsce/pkg/crossSum"
	"crsce/pkg/hashMatrix"
	"crsce/pkg/types"
	"os"
)

// Compress - perform CRSCE compression
func Compress(s types.MatrixPosition, block []byte, output *os.File) (err error) {

	LSM, VSM, DSM, XSM, LH := make(crossSum.Matrix, s), make(crossSum.Matrix, s),
		make(crossSum.Matrix, s), make(crossSum.Matrix, s), hashMatrix.New(s)

	// iterate over each bit in the block
	bytePos, bitPos := uint(0), uint8(0)
	for r := types.MatrixPosition(0); r < s; r++ {
		for c := types.MatrixPosition(0); c < s; c++ {
			var bit types.Bit
			currentByte := block[bytePos]
			if err = bit.FromByte(currentByte, bitPos); err == nil {
				return err
			}
			LSM.Push(r, bit)
			VSM.Push(c, bit)
			DSM.Push(common.DiagonalIndex(r, c, s), bit)
			XSM.Push(common.AntiDiagonalIndex(r, c, s), bit)
			LH.Push(r, c, bit)

			bitPos++
			if bitPos > 7 {
				bytePos++
				bitPos = 0
			}
			LH.CaptureHash(r)
		}
	}
	return SerializeOutput(output, LH, &LSM, &VSM, &DSM, &XSM)
}
