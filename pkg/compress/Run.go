package compress

import (
	"crsce/pkg/arguments"
	"crsce/pkg/types"
	"io"
	"os"
)

// Run - runs the compress operation
func Run(input, output *os.File) (err error) {
	s := types.MatrixPosition(*arguments.BlockSize)
	blockBuffer := make([]byte, s)

	// Read the file block-by-block
	for {
		// read the next block
		_, err = input.Read(blockBuffer)
		if err != nil {
			if err == io.EOF {
				break
			}
			return err
		}

		// For each block, compress the same and write to the output file
		if err = Compress(s, blockBuffer, output); err != nil {
			return err
		}
	}
	return nil
}
