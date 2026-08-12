#!/bin/bash

CLEAN_SCRIPT="./clean.sh"
BUILD_SCRIPT="./build.sh"
EXEC_SCRIPT="./exec.sh"


echo "Cleaning project artifacts for fresh build"
"$CLEAN_SCRIPT"
EXIT_CODE=$?

if [[ $EXIT_CODE -ne 0 ]]; then
	echo "failed to clean artifacts. Error code: $EXIT_CODE"

	exit $EXIT_CODE
else
	echo "Artifacts removed"
fi


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
