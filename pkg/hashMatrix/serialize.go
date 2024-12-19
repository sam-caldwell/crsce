package hashMatrix

import (
	"errors"
	"io"
)

// Serialize - serialize hash bits to output file
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
