package csm

// resize - a method for resizing the Simple CSM (used by New())
func (m *Simple) resize(size int) error {
	m.data = make([][]simpleElement, size)
	for i := range m.data {
		m.data[i] = make([]simpleElement, size)
	}
	return nil
}
