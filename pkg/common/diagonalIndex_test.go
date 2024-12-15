package common

import (
	"crsce/pkg/types"
	"testing"
)

func TestDiagonalIndex(t *testing.T) {
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

		{S: 4, R: 1, C: 0, I: 1},
		{S: 4, R: 1, C: 1, I: 2},
		{S: 4, R: 1, C: 2, I: 3},
		{S: 4, R: 1, C: 3, I: 0},

		{S: 4, R: 2, C: 0, I: 2},
		{S: 4, R: 2, C: 1, I: 3},
		{S: 4, R: 2, C: 2, I: 0},
		{S: 4, R: 2, C: 3, I: 1},

		{S: 4, R: 3, C: 0, I: 3},
		{S: 4, R: 3, C: 1, I: 0},
		{S: 4, R: 3, C: 2, I: 1},
		{S: 4, R: 3, C: 3, I: 2},

		{S: 5, R: 0, C: 0, I: 0},
		{S: 5, R: 0, C: 1, I: 1},
		{S: 5, R: 0, C: 2, I: 2},
		{S: 5, R: 0, C: 3, I: 3},
		{S: 5, R: 0, C: 4, I: 4},

		{S: 5, R: 1, C: 0, I: 1},
		{S: 5, R: 1, C: 1, I: 2},
		{S: 5, R: 1, C: 2, I: 3},
		{S: 5, R: 1, C: 3, I: 4},
		{S: 5, R: 1, C: 4, I: 0},

		{S: 5, R: 2, C: 0, I: 2},
		{S: 5, R: 2, C: 1, I: 3},
		{S: 5, R: 2, C: 2, I: 4},
		{S: 5, R: 2, C: 3, I: 0},
		{S: 5, R: 2, C: 4, I: 1},

		{S: 5, R: 3, C: 0, I: 3},
		{S: 5, R: 3, C: 1, I: 4},
		{S: 5, R: 3, C: 2, I: 0},
		{S: 5, R: 3, C: 3, I: 1},
		{S: 5, R: 3, C: 4, I: 2},

		{S: 5, R: 4, C: 0, I: 4},
		{S: 5, R: 4, C: 1, I: 0},
		{S: 5, R: 4, C: 2, I: 1},
		{S: 5, R: 4, C: 3, I: 2},
		{S: 5, R: 4, C: 4, I: 3},
	}

	for index, test := range tests {
		i := DiagonalIndex(test.R, test.C, test.S)
		if i != test.I {
			t.Fatalf("Test %d: Expected %d, got %d", index, test.I, i)
		}
	}
}
