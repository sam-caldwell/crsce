package bitFile

import "os"

type BitFile struct {
	fh             *os.File
	buffer         []byte
	bufferPosition int
}
