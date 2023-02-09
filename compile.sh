#!/bin/bash

cnt=`dpkg -l | grep build-essential | wc -l`
if [[ $cnt -eq 0 ]]; then
apt update
apt install -y build-essential cmake
apt-get install build-essential cmake zlib1g-dev libssl-dev libgtest-dev -y
fi

git submodule update --init --recursive
make

cd file_compare
make
