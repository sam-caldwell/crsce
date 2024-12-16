package main

import (
	"crsce/pkg/arguments"
	"fmt"
	"os"
)

func main() {
	if err := arguments.ParseArguments(); err != nil {
		fmt.Println(err.Error())
		os.Exit(1)
	}
	os.Exit(0)
}
