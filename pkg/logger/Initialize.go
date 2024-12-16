package logger

import (
	"crsce/pkg/arguments"
	"github.com/sam-caldwell/ansi"
	"os"
)

func Initialize() {
	var err error
	if *arguments.Debug {
		if debugFile, err = os.Create(*arguments.DebugFile); err != nil {
			ansi.Errorf("Failed to open debug file(%s): %v\n", *arguments.DebugFile, err).Fatal(1)
		}
		Write = func(msg string) {
			_, _ = debugFile.WriteString(msg)
		}
	} else {
		Write = func(msg string) { /* do nothing */ }
	}
}