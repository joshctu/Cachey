#!/bin/bash


cmake --build build
cd build && ./kv_store_init
