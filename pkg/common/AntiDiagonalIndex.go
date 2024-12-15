package common

import "crsce/pkg/types"

// AntiDiagonalIndex - Helper function to calculate anti-diagonal index
func AntiDiagonalIndex(r, c, s types.MatrixPosition) types.MatrixPosition {

	return (s - r + c - 1) % s

}
