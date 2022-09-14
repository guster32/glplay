#!/bin/bash

source ${ENVIRONMENT_SCRIPT}

#Ensure build dir exists
mkdir -p ${SRC_DIR}/crossbuild/debug

pushd ${SRC_DIR}/crossbuild/debug

#Clean build output directory
rm -rf ${SRC_DIR}/crossbuild/debug/* 

#Cmake
cmake -DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_C_FLAGS_DEBUG="-g -O0" \
-DCMAKE_CXX_FLAGS_DEBUG="-g -O0" \
-GNinja ../../
#-DCMAKE_CXX_COMPILER=${config:ARCADIA_QEMU.CXX}
#-DCMAKE_C_COMPILER=${config:ARCADIA_QEMU.CC}
#-DCMAKE_SYSROOT=${config:ARCADIA_QEMU.SDKTARGETSYSROOT}

ninja -v
