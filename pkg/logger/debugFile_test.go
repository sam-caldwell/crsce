package logger

import "testing"

func TestLogger_debugFile(t *testing.T) {
	if err := Initialize(); err != nil {
		t.Fatal(err)
	}
	if debugFile == nil {
		t.Fatal("expect debugFile to be not nil")
	}
}
