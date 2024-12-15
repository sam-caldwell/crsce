package bitFile

import "os"

type BitFile struct {
	fh *os.File

	buffer []byte

	bufferPosition uint

	bitPos uint8
}
