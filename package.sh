#!/bin/bash

mkdir -p /opt/pkg/_lib
mkdir -p /opt/pkg/workflow/_lib/
mkdir -p /opt/pkg/_lib/workflow/_lib
mkdir -p /opt/pkg/file_compare
mkdir -p ./workflow/_lib

cp -rfL ./_lib/libwfrest.so /opt/pkg/_lib/libwfrest.so
cp -rfL ./workflow/_lib/libworkflow.so  /opt/pkg/_lib/workflow/_lib/libworkflow.so
cp -rfL ./file_compare/main /opt/pkg/file_compare/main
cp -rf Dockerfile /opt/pkg/
cp -rfL ./workflow/_lib/libworkflow.so.0 /opt/pkg/workflow/_lib/libworkflow.so.0

cd /opt/pkg
docker build . -t file_compare
