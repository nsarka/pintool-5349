#!/bin/bash

# Folders
CORUNDUM=/optane/sarkauskas.1/Corundum-extensions
PIN=/optane/sarkauskas.1/pin
APP=/optane/sarkauskas.1/tmp_proj1

# Set a var to the application binary inside of APP folder
BINARY="$APP/target/debug/hello-rust set 110"

# Run the app with the pintool
PIN_ROOT=$PIN $PIN/pin -t ./obj-intel64/pmem.so -- $BINARY

