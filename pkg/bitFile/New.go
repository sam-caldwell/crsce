package bitFile

import "os"

const (
	bufferSize = 1024
)

func New(fileName *string) (result *BitFile, err error) {
	result = &(BitFile{
		fh: func() (h *os.File) {
			h, err = os.Open(*fileName)
			return h
		}(),
		buffer:         make([]byte, bufferSize),
		bufferPosition: 0,
	})
	return result, err
}
