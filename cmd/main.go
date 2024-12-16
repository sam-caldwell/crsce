package main

import (
	"crsce/pkg/arguments"
	"crsce/pkg/compress"
	"crsce/pkg/logger"
	"github.com/sam-caldwell/ansi"
	"os"
)

func main() {
	var (
		err           error
		input, output *os.File
	)
	arguments.ParseArguments()

	if *arguments.Debug {
		ansi.Green().Println("starting...").Reset()
	}

	// cleanup any loggers we may create
	defer logger.Close()

	if input, err = os.Open(*arguments.InFile); err != nil {
		ansi.Errorf("Failed to open input file: %v\n", err).Reset()
		return
	}
	defer swallowError(input.Close())

	if output, err = os.Open(*arguments.InFile); err != nil {
		ansi.Errorf("Failed to open output file: %v\n", err).Reset()
		return
	}
	defer swallowError(output.Close())

	switch *arguments.Mode {
	case "compress":

		if err := compress.Run(input, output); err != nil {
			ansi.Errorln(err).Fatal(1)
		}

	case "decompress":

		//if err := decompress.Run(input, output); err != nil {
		//	ansi.Errorln(err).Fatal(1)
		//}

	default:

		ansi.Errorf("Invalid mode:'%s'\n", *arguments.Mode).Fatal(1)

	}

	if *arguments.Debug {
		ansi.Green().
			Printf("Debug Info: %s\n", *arguments.DebugFile).
			Println("done").
			Reset()
	}

}

func swallowError(_ error) {
	// do nothing
}
