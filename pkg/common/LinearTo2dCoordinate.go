package common

import "crsce/pkg/types"

// LinearTo2dCoordinate - convert a linear address (i) to a two-dimensional coordinate pair (r,c) based on a size (s)
func LinearTo2dCoordinate(i, s types.MatrixPosition) (r, c types.MatrixPosition) {
	r = i / s // Row index
	c = i % s // Column index
	return r, c
}
