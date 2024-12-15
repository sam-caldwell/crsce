package types

import "fmt"

// FromByte - given a byte value and desired bit position, set the Bit to the corresponding bit value
func (b *Bit) FromByte(byteValue byte, bitPosition uint8) error {
	if bitPosition > 7 {
		return fmt.Errorf("bit position out of range (%d)", bitPosition)
	}
	*b = Bit(byteValue & (1 << bitPosition) >> bitPosition)
	return nil
}
