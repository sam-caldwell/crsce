package crossSum

import "testing"

func TestMatrix_New(t *testing.T) {
	t.Run("test size 0", func(t *testing.T) {
		defer func() {
			recover()
		}()
		m := New(0)
		if len(m) != 0 {
			t.Errorf("Matrix must be zero length")
		}
	})
	t.Run("test size 1", func(t *testing.T) {
		for i := 1; i < 10; i++ {
			m := New(uint(i))
			if len(m) != i {
				t.Fatalf("size %d expected", i)
			}
		}
	})
}
