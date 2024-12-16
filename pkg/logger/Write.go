package logger

// Write - exported log writer
var Write func(msg string) = func(msg string) { /* do nothing */ }
