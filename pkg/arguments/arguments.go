package arguments

import (
	"flag"
	"fmt"
	"github.com/sam-caldwell/file"
)

const (
	minimumBlockSize = 292
)

var (
	Mode      = flag.String("mode", "", "select compress, decompress mode")
	InFile    = flag.String("in", "", "input file")
	OutFile   = flag.String("out", "", "output file")
	DebugFile = flag.String("debug-file", "debug.txt", "a file containing debug info")
	Debug     = flag.Bool("debug", false, "debug output")
	Force     = flag.Bool("force", false, "force overwrite")
	BlockSize = flag.Uint("block-size", 512, "block size")
)

func ParseArguments() error {

	flag.Parse()

	if _, ok := map[string]any{"compress": nil, "decompress": nil}[*Mode]; !ok {
		return fmt.Errorf("invalid mode:'%s'", *Mode)
	}

	if *InFile == "" {
		return fmt.Errorf("must provide input file")
	}

	if *OutFile == "" {
		return fmt.Errorf("must provide output file")
	}

	if *Debug && *DebugFile != "" {
		return fmt.Errorf("if -debug is used, a -debug-file must be specified")
	}

	if !file.Exists(*InFile) {
		return fmt.Errorf("input file '%s' does not exist", *InFile)
	}

	if !*Force && file.Exists(*OutFile) {
		return fmt.Errorf("output file '%s' already exists (use -force to overwrite)", *OutFile)
	}

	if *BlockSize < minimumBlockSize {
		return fmt.Errorf("block size must be at least %d bytes", minimumBlockSize)
	}
	return nil
}
