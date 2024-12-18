package hashMatrix

import "bytes"

// Equal - returns true if two Hashes are equal
func (lhs *Hash) Equal(rhs Hash) bool {

	return bytes.Equal(lhs[:], rhs[:])

}
