package logger_test

import (
	"crsce/pkg/logger"
	"testing"
)

func TestLogger_Close(t *testing.T) {

	t.Run("test repeated close() calls and verify write is adjusted properly", func(t *testing.T) {
		logger.Initialize()
		logger.Close()
		logger.Initialize()
		logger.Close()
		logger.Write("noop")
	})
}
