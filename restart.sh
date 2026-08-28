#!/bin/bash

BUILD_SCRIPT="./build.sh"
EXEC_SCRIPT="./exec.sh"

echo "Running project with existing artifacts"

echo "Building executable"
"$BUILD_SCRIPT"
$EXIT_CODE=$?

if [[ $EXIT_CODE -ne 0 ]]; then
	echo "Failed to build executable. Error code: $EXIT_CODE"

	exit $EXIT_CODE
else
	echo "Project built successfully"
fi

echo "Running project"
"$EXEC_SCRIPT"
