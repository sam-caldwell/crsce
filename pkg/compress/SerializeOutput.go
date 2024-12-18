package compress

import (
	"crsce/pkg/crossSum"
	"crsce/pkg/hashMatrix"
	"os"
)

func SerializeOutput(output *os.File, LH *hashMatrix.Matrix, LSM, VSM, DSM, XSM *crossSum.Matrix) (err error) {
	const b = 9 //b=log2(s). Where s=512, b=9
	if err = LH.Serialize(output); err != nil {
		return err
	}
	if err = LSM.Serialize(output, b); err != nil {
		return err
	}
	if err = VSM.Serialize(output, b); err != nil {
		return err
	}
	if err = DSM.Serialize(output, b); err != nil {
		return err
	}
	if err = XSM.Serialize(output, b); err != nil {
		return err
	}
	return err
}
