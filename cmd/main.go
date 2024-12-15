package main

import (
	"crsce/pkg/common/arguments"
	"crsce/pkg/compress"
	"crsce/pkg/decompress"
	"github.com/sam-caldwell/ansi"
)

func main() {

	if *arguments.Debug {
		ansi.Green().Println("starting...").Reset()
	}

	switch *arguments.Mode {
	case "compress":

		if err := compress.Run(); err != nil {
			ansi.Errorln(err).Fatal(1)
		}

	case "decompress":

		if err := decompress.Run(); err != nil {
			ansi.Errorln(err).Fatal(1)
		}

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
