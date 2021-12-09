#!/bin/bash

CORUNDUM=/optane/sarkauskas.1/Corundum-extensions
PIN=/optane/sarkauskas.1/pin
APP=/optane/sarkauskas.1/tmp_proj1

# Build corundum with the patch for writing to /tmp/pmem.txt in open_impl
pushd .
cp ./corundum.diff $CORUNDUM
cd $CORUNDUM
git checkout .
git apply ./corundum.diff
cargo build
popd

# Build the application
pushd .
cd $APP
cargo build
popd

# Build the pintool
PIN_ROOT=$PIN make
