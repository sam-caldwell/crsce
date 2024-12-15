package logger

// Close - close the log file
func Close() {
	if debugFile != nil {
		_ = debugFile.Close()
	}
}
