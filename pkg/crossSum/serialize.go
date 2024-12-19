package crossSum

import (
	"errors"
	"github.com/sam-caldwell/ansi"
	"io"
)

// Serialize - serialize the cross sum matrix as b-bit elements to an output file.
// Each element of the matrix (of type uint16) will be packed into a bitstream such
// that each element uses exactly b bits.
func (m *Matrix) Serialize(output io.Writer, b uint16) (err error) {
	var (
		//
		byteBuffer     = make([]byte, 2*len(*m)+1)
		bufferPosition uint
		bitPosition    uint8 = 1
		resultWidth    uint
	)
	if b > 16 {
		return errors.New("b must be less than 16")
	}
	defer ansi.Reset()
	// iterate over each sum in the cross sum matrix (m)
	// note: each sum is a uint16...
	for sumIndex, sum := range *m {
		ansi.Blue().Printf("index(%d) serialize sum(%08b)", sumIndex, sum).LF()
		//iterate over each bit in the 16-bit sum and pack the bit
		for i := uint16(0); i < b; i++ {
			n := 0x0F - i // n is the bit index from left
			ansi.Printf("i/b(n):%d/%d(%d) buffer:%08b", i, b, n, byteBuffer)
			mask := uint16(1 << n)
			bit := (sum & mask) >> n
			//store the bit on the buffer as least-significant bit
			byteBuffer[bufferPosition] |= byte(bit)
			// check our byte alignment
			if (bitPosition % 8) == 0 {
				ansi.Printf("bitPosition (%d) aligned.  Incrementing bufferPosition to %d", bitPosition, bufferPosition)
				// if our bit position is at a byte boundary, move the bufferPosition to the next byte
				bufferPosition++
				bitPosition = 1
				resultWidth++
			} else {
				ansi.Printf("bitPosition %d", bitPosition)
				// if our bit position is not at a byte boundary, increment it and shift the bits in byteBuffer left
				byteBuffer[bufferPosition] <<= 1
				bitPosition++
			}
			ansi.LF()
		}
	}
	if b < 16 {
		resultWidth++
	}
	ansi.Green().Printf("Serialization complete.  Buffer:%08b", byteBuffer[0:resultWidth])
	if _, err := output.Write(byteBuffer[0:resultWidth]); err != nil {
		return err
	}
	return nil
}
