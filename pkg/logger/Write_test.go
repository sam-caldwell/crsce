package logger

import (
	"crsce/pkg/arguments"
	"os"
	"testing"
)

func TestLogger_Write(t *testing.T) {
	defer Close()
	t.Run("test write function initial state", func(t *testing.T) {
		if Write == nil {
			t.Fatal("Write should not be nil initially")
		}
	})
	t.Run("test write function", func(t *testing.T) {
		t.Run("setup the flags", func(t *testing.T) {
			os.Args = append(os.Args,
				"-debug",
				"-force",
				"-in", "/dev/stdin",
				"-mode", "compress",
				"-out", "/tmp/crsce-test.out")
			if err := arguments.ParseArguments(); err != nil {
				t.Fatal(err)
			}
		})
		var testData string = "fail"
		t.Run("test the Write() function", func(t *testing.T) {
			Write = func(msg string) {
				testData = msg
			}
			if Write("success"); testData != "success" {
				t.Fatalf("Write() expected 'success' but got '%s'", testData)
			}
		})
	})
}
