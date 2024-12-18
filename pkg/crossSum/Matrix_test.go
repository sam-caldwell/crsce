package crossSum

import (
	"math"
	"reflect"
	"testing"
)

func TestCrossSum_Matrix(t *testing.T) {
	t.Run("test matrix list type", func(t *testing.T) {
		var matrix = Matrix{0, math.MaxUint16}
		actual := reflect.TypeOf(matrix)
		if actual.Kind() != reflect.Slice {
			t.Fatalf("Matrix is not an array")
		}
	})
	t.Run("test matrix list element type", func(t *testing.T) {
		var matrix = Matrix{0, math.MaxUint16}
		actual := reflect.TypeOf(matrix[0])
		if actual.Kind() != reflect.Uint16 {
			t.Fatalf("Matrix is not an Uint16")
		}

	})
}
