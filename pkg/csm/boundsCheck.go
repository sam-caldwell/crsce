package csm

import (
	"crsce/pkg/types"
	"fmt"
)

func boundsCheck(matrix Interface, r, c types.MatrixPosition) error {

	if sz := matrix.size(); r >= sz || c >= sz {

		return fmt.Errorf("bounds check failed (r:%d, c:%d, sz:%d)", r, c, sz)

	}

	return nil

}
