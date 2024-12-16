package logger

import (
	"crsce/pkg/arguments"
	"fmt"
	"github.com/sam-caldwell/ansi"
	"os"
)

// Initialize sets up the logger based on the debug flag.
func Initialize() error {
	// Ensure DebugFile is valid before proceeding
	if arguments.DebugFile == nil {
		return fmt.Errorf("DebugFile argument is not set")
	}

	if *arguments.Debug {
		// Attempt to create the debug file
		var err error
		debugFile, err = os.Create(*arguments.DebugFile)
		if err != nil {
			return fmt.Errorf("failed to open debug file (%s): %v", *arguments.DebugFile, err)
		}

		// Assign the Write function to write to the debug file
		Write = func(msg string) {
			writeMutex.Lock()
			defer writeMutex.Unlock()
			_, _ = debugFile.WriteString(msg + "\n")
		}

		// Inform the user that logging is initialized
		ansi.Red().Printf("log file: %s\n", *arguments.DebugFile).Reset()
		Write(fmt.Sprintf("logger initialized (file: %s)", *arguments.DebugFile))
	} else {
		// Assign a no-op Write function when debugging is disabled
		Write = func(msg string) { /* no-op */ }
	}

	return nil
}
