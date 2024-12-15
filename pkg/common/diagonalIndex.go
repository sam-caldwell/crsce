package common

import "crsce/pkg/types"

// DiagonalIndex - Helper function to calculate diagonal index
func DiagonalIndex(r, c, s types.MatrixPosition) types.MatrixPosition {

	return (r + c) % s

}
