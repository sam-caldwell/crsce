package arguments

import (
	"flag"
	"github.com/sam-caldwell/ansi"
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

func init() {

	flag.Parse()

	if *InFile == "" {
		ansi.Errorln("no input file specified").Fatal(1)
	}

	if *OutFile == "" {
		ansi.Errorln("no output file specified").Fatal(1)
	}

	if *Debug && *DebugFile != "" {
		ansi.Errorln("debug output and debug file specified").Fatal(1)
	}

	if !file.Exists(*InFile) {
		ansi.Errorf("file not found: %s\n", *InFile).Fatal(1)
	}
	if !*Force && file.Exists(*OutFile) {
		ansi.Errorf("file already exists: %s (use -force to overwrite)\n", *OutFile).Fatal(1)
	}
	if *BlockSize < minimumBlockSize {
		ansi.Errorf("block size must be at least %d bits\n", minimumBlockSize).Fatal(1)
	}
}
