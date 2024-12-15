package compress

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

	if err := Compress(input, output); err != nil {
		return err
	}
	return nil
}

func Compress(input *bitFile.BitFile, output *bitFile.BitFile) (err error) {
	return nil
}
