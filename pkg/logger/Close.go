package logger

// Close - close the log file
func Close() {
	if debugFile != nil {
		Write = func(_ string) { /*do nothing*/ }
		_ = debugFile.Close()
	}
}
