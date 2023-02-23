#!/bin/bash

mkdir -p /opt/fc_pkg/_lib
mkdir -p /opt/fc_pkg/workflow/_lib/
mkdir -p /opt/fc_pkg/_lib/workflow/_lib
mkdir -p /opt/fc_pkg/file_compare
mkdir -p ./workflow/_lib

cp -rf /opt/fc/file_compare/README.md /opt/fc_pkg/file_compare/README.md
cp -rfL ./_lib/libwfrest.so /opt/fc_pkg/_lib/libwfrest.so
cp -rfL ./workflow/_lib/libworkflow.so  /opt/fc_pkg/_lib/workflow/_lib/libworkflow.so
cp -rfL ./file_compare/main /opt/fc_pkg/file_compare/main
cp -rfL ./file_compare/test_post /opt/fc_pkg/file_compare/test_post
cp -rfL ./file_compare/test_get /opt/fc_pkg/file_compare/test_get
cp -rfL ./file_compare/input.json /opt/fc_pkg/file_compare/input.json
cp -rfL ./workflow/_lib/libworkflow.so.0 /opt/fc_pkg/workflow/_lib/libworkflow.so.0

cd /opt/fc_pkg

#docker build . -t file_compare

cd /tmp
mv /opt/fc_pkg ./fc

ts=`date +"%Y%m%d"`
cnt=`ls /tmp/ | grep fc_pkg | grep ${ts} | wc -l`
let cnt=$((cnt+1))
tar cjf file_compare-${ts}-v${cnt}.tar.bz2 fc
