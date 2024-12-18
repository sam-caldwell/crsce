package hashMatrix

import (
	"math/rand"
	"time"
)

// generateRandomString - generate a random string
func generateRandomString(n int) string {
	const charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	if n <= 0 {
		return ""
	}
	seededRand := rand.New(rand.NewSource(time.Now().UnixNano()))
	result := make([]byte, n)

	for i := range result {
		result[i] = charset[seededRand.Intn(len(charset))]
	}
	return string(result)
}
