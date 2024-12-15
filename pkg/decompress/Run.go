package decompress

import (
	"crsce/pkg/arguments"
	"crsce/pkg/bitFile"
)

// Run - runs the compress operation
func Run() (err error) {
	input, err := bitFile.New(arguments.InFile)
	if err != nil {
		return err
	}
	output, err := bitFile.New(arguments.OutFile)
	if err != nil {
		return err
	}

	if err := Decompress(input, output); err != nil {
		return err
	}
	return nil
}

func Decompress(input *bitFile.BitFile, output *bitFile.BitFile) (err error) {
	return nil
}
