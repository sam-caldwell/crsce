package bitFile

import "crsce/pkg/types"

// Read - read a single bit from the file on a FIFO basis from the current file position
func (b *BitFile) Read() (bit types.Bit, err error) {
	if err = b.loadBufferIfConsumed(); err != nil {
		return bit, err
	}

	if err = bit.FromByte(b.getCurrentByte(), b.bitPos); err != nil {
		return bit, err
	}

	b.IncrementBitPosition()

	return bit, err
}
