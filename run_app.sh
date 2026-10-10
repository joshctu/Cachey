#!/bin/bash


cmake --build build
./build/kv_store_init &
STORE_ID=$!
echo "Store id: $STORE_ID"
python3 src/network/python_client/python_client.py
echo "we returned from python client"
kill -9 "$STORE_ID"
