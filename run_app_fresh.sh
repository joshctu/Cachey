#!/bin/bash


CLEAN_SCRIPT="./scripts/clean.sh"

echo "Cleaning project artifacts"
"$CLEAN_SCRIPT"
EXIT_CODE=$?

if [[ $EXIT_CODE -ne 0 ]]; then
	echo "Artifacts could not be cleaned up. Exiting with code: $EXIT_CODE"

	exit $EXIT_CODE

else
	echo "Artifacts successfully cleaned up"
fi

cmake --build build
cd build && ./kv_store_init

