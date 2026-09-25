#!/bin/bash

BUILD_SCRIPT="./build.sh"
CLEAN_SCRIPT="./clean.sh"
TEST_SCRIPT="./run_tests.sh"

echo "Cleaning project artifacts for fresh build"
"$CLEAN_SCRIPT"
EXIT_CODE=$?

if [[$EXIT_CODE -ne 0]]; then
	echo "Failed to clean artifacts. Exit code: $EXIT_CODE"

	exit $EXIT_CODE

else
	echo "Artifacts cleaned up"
fi

echo "Building project"
"$BUILD_SCRIPT"
EXIT_CODE=$?

if [[$EXIT_CODE -ne 0]]; then
	echo "Project failed to build: Exit code: $EXIT_CODE"

	exit $EXIT_CODE

else
	echo "Project built successfully"
fi

echo "Running tests"
"$TEST_SCRIPT"
EXIT_CODE=$?

if [[$EXIT_CODE -ne 0]]; then
	echo "Project failed to run: Exit code: $EXIT_CODE"

	exit $EXIT_CODE

else
	echo "Project ran successfully"
fi





