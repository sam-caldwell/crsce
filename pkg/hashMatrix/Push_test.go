package hashMatrix

import (
	"bytes"
	"crsce/pkg/types"
	"testing"
)

func TestMatrix_Push(t *testing.T) {
	t.Run("test bounds checks", func(t *testing.T) {
		t.Run("test bounds check (r)", func(t *testing.T) {
			m := Matrix{
				buffer:         nil,
				bufferPosition: 0,
				bitPosition:    0,
				size:           10,
				hash:           nil,
			}
			defer func() {
				recover()
			}()
			m.Push(20, 0, 0)
		})
		t.Run("test bounds check (c)", func(t *testing.T) {
			m := Matrix{
				buffer:         nil,
				bufferPosition: 0,
				bitPosition:    0,
				size:           10,
				hash:           nil,
			}
			crash := true
			defer func() {
				recover()
				crash = false
			}()
			m.Push(0, 20, 0)
			if crash {
				t.Fatal("expected bounds check fail")
			}
		})
	})
	t.Run("test bit values", func(t *testing.T) {
		t.Run("test bit value validation", func(t *testing.T) {
			m := Matrix{
				buffer:         make([]byte, 1),
				bufferPosition: 0,
				bitPosition:    0,
				size:           1,
				hash:           nil,
			}
			crash := true
			defer func() {
				recover()
				crash = false
			}()
			const invalidBitValue = 2
			m.Push(0, 0, invalidBitValue)
			if crash {
				t.Fatal("expected bounds check fail")
			}
		})
		t.Run("valid bit value(0)", func(t *testing.T) {
			m := Matrix{
				buffer:         make([]byte, 1),
				bufferPosition: 0,
				bitPosition:    0,
				size:           1,
				hash:           nil,
			}
			defer func() {
				if err := recover(); err != nil {
					t.Fatalf("unexpected failure (1): %v", err)
				}
			}()
			m.Push(0, 0, 0)
		})
		t.Run("valid bit value(1)", func(t *testing.T) {
			m := Matrix{
				buffer:         make([]byte, 1),
				bufferPosition: 0,
				bitPosition:    0,
				size:           1,
				hash:           nil,
			}
			defer func() {
				if err := recover(); err != nil {
					t.Fatalf("unexpected failure (1): %v", err)
				}
			}()
			m.Push(0, 0, 1)
		})
	})
	t.Run("test buffer storage", func(t *testing.T) {
		m := Matrix{
			buffer:         make([]byte, 2),
			bufferPosition: 0,
			bitPosition:    0,
			size:           2,
			hash:           nil,
		}
		checkState := func(r, c, size types.MatrixPosition, bufferPos uint, bitPos uint8, buffer []byte) {
			if m.bufferPosition != bufferPos {
				t.Fatalf("Failed (bufferPos) (r:%d,c:%d,s:%d)\n"+
					"pos:\n"+
					"    buffer: (expected: %02d, actual: %02d)"+
					"       bit: (expected: %02d, actual: %02d)\n"+
					"buffer:(\n"+
					"    expect: %08b\n"+
					"    actual: %08b)",
					r, c, size,
					bufferPos, m.bufferPosition,
					bitPos, m.bitPosition,
					buffer, m.buffer)
			}
			if m.bitPosition != bitPos {
				t.Fatalf("Failed (bitPos) (r:%d,c:%d,s:%d)\n"+
					"pos:\n"+
					"    buffer: (expected: %02d, actual: %02d)"+
					"       bit: (expected: %02d, actual: %02d)\n"+
					"buffer:(\n"+
					"    expect: %08b\n"+
					"    actual: %08b)",
					r, c, size,
					bufferPos, m.bufferPosition,
					bitPos, m.bitPosition,
					buffer, m.buffer)
			}
			if m.size != size {
				t.Fatalf("Failed (size) (r:%d,c:%d,s:%d)\n"+
					"pos:\n"+
					"    buffer: (expected: %02d, actual: %02d)"+
					"       bit: (expected: %02d, actual: %02d)\n"+
					"buffer:(\n"+
					"    expect: %08b\n"+
					"    actual: %08b)",
					r, c, size,
					bufferPos, m.bufferPosition,
					bitPos, m.bitPosition,
					buffer, m.buffer)
			}
			if !bytes.Equal(buffer, m.buffer) {
				t.Fatalf("Failed (content) (r:%d,c:%d,s:%d)\n"+
					"pos: (buf:%d,bit:%d)\n"+
					"buffer:(\n"+
					"    expect: %08b\n"+
					"    actual: %08b)",
					r, c, size, bufferPos, bitPos, buffer, m.buffer)
			}
		}
		testFunc := func(r, c, size types.MatrixPosition, bit types.Bit, bufferPos uint, bitPos uint8, buffer []byte) {
			m.Push(r, c, bit)
			checkState(r, c, size, bufferPos, bitPos, buffer)
		}
		testFunc(0, 0, 2, 1, 0, 1, []byte{0b10000000, 0b00000000})
		testFunc(0, 1, 2, 1, 0, 2, []byte{0b11000000, 0b00000000})
		testFunc(0, 2, 2, 1, 0, 3, []byte{0b11100000, 0b00000000})
		testFunc(0, 3, 2, 1, 0, 4, []byte{0b11110000, 0b00000000})

		testFunc(0, 4, 2, 1, 0, 5, []byte{0b11111000, 0b00000000})
		testFunc(0, 5, 2, 1, 0, 6, []byte{0b11111100, 0b00000000})
		testFunc(0, 6, 2, 1, 0, 7, []byte{0b11111110, 0b00000000})
		testFunc(0, 7, 2, 1, 1, 0, []byte{0b11111111, 0b00000000})

		testFunc(1, 0, 2, 1, 1, 1, []byte{0b11111111, 0b10000000})
		testFunc(1, 1, 2, 1, 1, 2, []byte{0b11111111, 0b11000000})
		testFunc(1, 2, 2, 1, 1, 3, []byte{0b11111111, 0b11100000})
		testFunc(1, 3, 2, 1, 1, 4, []byte{0b11111111, 0b11110000})
	})
}
