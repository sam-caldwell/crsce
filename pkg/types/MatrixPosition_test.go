package types

import (
	"math"
	"testing"
)

func TestMatrixPosition(t *testing.T) {
	t.Run("min value", func(t *testing.T) {
		_ = MatrixPosition(0)
	})
	t.Run("max value", func(t *testing.T) {
		_ = MatrixPosition(math.MaxUint)
	})
}
