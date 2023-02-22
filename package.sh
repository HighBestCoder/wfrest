#!/bin/bash

mkdir -p /opt/pkg/_lib
mkdir -p /opt/pkg/workflow/_lib/
mkdir -p /opt/pkg/_lib/workflow/_lib
mkdir -p /opt/pkg/file_compare
mkdir -p ./workflow/_lib

cp -rf /opt/fc/file_compare/README.md /opt/pkg/file_compare/README.md
cp -rfL ./_lib/libwfrest.so /opt/pkg/_lib/libwfrest.so
cp -rfL ./workflow/_lib/libworkflow.so  /opt/pkg/_lib/workflow/_lib/libworkflow.so
cp -rfL ./file_compare/main /opt/pkg/file_compare/main
cp -rfL ./file_compare/post /opt/pkg/file_compare/post
cp -rfL ./file_compare/get /opt/pkg/file_compare/get
cp -rfL ./file_compare/input.json /opt/pkg/file_compare/input.json
cp -rf Dockerfile /opt/pkg/
cp -rfL ./workflow/_lib/libworkflow.so.0 /opt/pkg/workflow/_lib/libworkflow.so.0

cd /opt/pkg

docker build . -t file_compare

cd /opt

ts=`date +"%Y%m%d"`
cnt=`ls /opt/ | grep kg | grep ${ts} | wc -l`
let cnt=$((cnt+1))
tar cjf file_compare-${ts}-v${cnt}.tar.bz2 /opt/pkg
