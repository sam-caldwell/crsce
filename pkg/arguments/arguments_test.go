package arguments

import (
	"bytes"
	"fmt"
	"os/exec"
	"strings"
	"testing"
)

func TestArguments(t *testing.T) {
	type TestData []struct {
		arg    string
		argV   string
		output string
	}
	runTest := func(arg, argV string) string {
		args := map[string]string{
			"mode":  "compress",
			"in":    "/dev/stdin",
			"out":   "/tmp/test.out",
			"force": "true",
		}

		// Upsert the given input argument under test
		args[arg] = argV

		// Construct the argument list
		var argList = []string{
			"run",
			"./testHelpers/arguments.go",
		}
		for k, v := range args {
			argList = append(argList, fmt.Sprintf("-%s=%s", k, v))
		}

		// Create the command
		cmd := exec.Command("go", argList...)

		// Capture stdout and stderr
		var stdout bytes.Buffer
		cmd.Stdout = &stdout
		// Run the command
		if err := cmd.Run(); err != nil {
			t.Logf("args: %v", argList)
			t.Logf("stdout(%s)", stdout.String())
			return strings.TrimPrefix(strings.TrimSuffix(stdout.String(), "\n"), "\x1b[0m")
		}
		return ""
	}

	t.Run("test minimumBlockSize", func(t *testing.T) {
		if minimumBlockSize != 292 {
			t.Fatal("minimumBlockSize should be 292")
		}
	})
	t.Run("test the arguments", func(t *testing.T) {
		testData := TestData{
			{
				"-mode",
				"compress",
				"",
			},
			{
				"-mode",
				"decompress",
				"",
			},
			{
				"-mode",
				"",
				"invalid mode:''",
			},
			{
				"-mode",
				"bad",
				"invalid mode:'bad'",
			},
			{
				"-in",
				"foo.non-existent.file.bar",
				"input file 'foo.non-existent.file.bar' does not exist",
			},
		}
		for index, test := range testData {
			t.Logf("test %d:('%s':%s) expect(%s)", index, test.arg, test.argV, test.output)
			if output := runTest(test.arg, test.argV); output != "" && output != test.output {
				t.Fatalf("test %d \n"+
					"expected (%s) %v\n"+
					" but got (%s) %v",
					index,
					test.output, []byte(test.output),
					output, []byte(output))
			}
		}
	})
}
