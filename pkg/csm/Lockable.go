package csm

// Lockable - a CSM Matrix that consists of data and locks for decompression
type Lockable struct {
	data [][]LockingElement
}
