package hashMatrix

import (
	"errors"
	"io"
)

// Serialize - serialize hash bits to output file.
//
// Because a Hash is byte-aligned, we should be serializing this first to the output,
// and thus, we should push the bytes directly to the io.Writer output stream without
// the more complex bit packing process used by the cross sum matrices.
func (m *Matrix) Serialize(output io.Writer) (err error) {
	if output == nil {
		return errors.New("file handle is nil")
	}
	m.lock.Lock()
	defer m.lock.Unlock()

	for _, hash := range m.hash {
		if _, err := output.Write(hash.Serialize()); err != nil {
			return err
		}
	}

	return nil

}
