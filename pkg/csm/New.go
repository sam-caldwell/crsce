package csm

import "crsce/pkg/types"

// New - return an initialized CSM Matrix object (either simple or lockable)
func New(locking bool, size types.MatrixPosition) (matrix Interface, err error) {

	if locking {

		matrix = &(Simple{})

	} else {

		matrix = &(Lockable{})

	}

	if err = matrix.resize(size); err != nil {

		return matrix, err

	}

	return matrix, nil

}
