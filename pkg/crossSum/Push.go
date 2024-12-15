package crossSum

import "crsce/pkg/types"

func (m *Matrix) Push(i types.MatrixPosition, bit types.Bit) {

	(*m)[i] += uint16(bit)

}
