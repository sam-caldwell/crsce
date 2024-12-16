package logger

import (
	"crsce/pkg/arguments"
	"github.com/sam-caldwell/file"
	"os"
	"testing"
)

func TestLogger_Initialize(t *testing.T) {
	os.Args = append(os.Args, "-debug", "-mode", "compress",
		"-in", "/dev/stdin", "-out", "/tmp/crsce-test.out")
	if err := arguments.ParseArguments(); err != nil {
		t.Error(err)
	}
	defer Close()
	t.Run("initial conditions", func(t *testing.T) {
		//expect noop with no error
		Write("noop")
		// make sure any pre-existing debugFile does not exist
		if file.Exists(*arguments.DebugFile) {
			if err := os.Remove(*arguments.DebugFile); err != nil {
				t.Fatal(err)
			}
		}
	})
	t.Run("initialize", func(t *testing.T) {
		if err := Initialize(); err != nil {
			t.Fatal(err)
		}
		Write("test")
		Close()
	})
	t.Run("final conditions", func(t *testing.T) {
		if !file.Exists(*arguments.DebugFile) {
			t.Fatal("expected debug file exists after logger initialized")
		}
	})
}
