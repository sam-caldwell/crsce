package logger_test

import (
	"crsce/pkg/logger"
	"testing"
)

func TestLogger_Close(t *testing.T) {

	t.Run("test repeated close() calls and verify write is adjusted properly", func(t *testing.T) {
		if err := logger.Initialize(); err != nil {
			t.Fatal(err)
		}
		logger.Close()
		if err := logger.Initialize(); err != nil {
			t.Fatal(err)
		}
		logger.Close()
		logger.Write("noop")
	})
}
