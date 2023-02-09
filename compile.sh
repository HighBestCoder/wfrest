#!/bin/bash

git submodule update --init --recursive
make

cd file_compare
make
