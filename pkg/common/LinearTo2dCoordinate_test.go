package common

import (
	"crsce/pkg/types"
	"testing"
)

func TestLinearTo2dCoordinate(t *testing.T) {
	tests := []struct {
		S types.MatrixPosition
		I types.MatrixPosition
		R types.MatrixPosition
		C types.MatrixPosition
	}{
		{S: 4, R: 0, C: 0, I: 0},
		{S: 4, R: 0, C: 1, I: 1},
		{S: 4, R: 0, C: 2, I: 2},
		{S: 4, R: 0, C: 3, I: 3},

		{S: 4, R: 1, C: 0, I: 4},
		{S: 4, R: 1, C: 1, I: 5},
		{S: 4, R: 1, C: 2, I: 6},
		{S: 4, R: 1, C: 3, I: 7},

		{S: 4, R: 2, C: 0, I: 8},
		{S: 4, R: 2, C: 1, I: 9},
		{S: 4, R: 2, C: 2, I: 10},
		{S: 4, R: 2, C: 3, I: 11},

		{S: 4, R: 3, C: 0, I: 12},
		{S: 4, R: 3, C: 1, I: 13},
		{S: 4, R: 3, C: 2, I: 14},
		{S: 4, R: 3, C: 3, I: 15},
	}

	for index, test := range tests {
		r, c := LinearTo2dCoordinate(test.I, test.S)
		if r != test.R || c != test.C {
			t.Fatalf("n(%d) LinearTo2dCoordinate(%d, %d) = (r=%d, c=%d); want (r=%d, c=%d)",
				index, test.I, test.S, r, c, test.R, test.C)
		}
	}
}
