package logger

import "sync"

var (
	// Write - exported log writer
	Write func(msg string) = func(msg string) { /* do nothing */ }
	// writeMutex - Mutex to ensure thread-safe writes
	writeMutex sync.Mutex
)
