#!/bin/sh

set -x

SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-./build}
BUILD_TYPE=${BUILD_TYPE:-release}
INSTALL_DIR=${INSTALL_DIR:-../${BUILD_TYPE}-install}
BUILD_NO_SOAP_SERVER=${BUILD_NO_SOAP_SERVER:-0}

MUDUOX_DIR=${MUDUOX_DIR:-../muduox}
LIBUV_DIR=${LIBUV_DIR:-../libuv}
LIBRE_DIR=${LIBRE_DIR:-../libre}
TINYXML2_DIR=${TINYXML2_DIR:-../tinyxml2}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
  && cd $BUILD_DIR/$BUILD_TYPE \
  && cmake \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           -DCMAKE_BUILD_NO_SOAP_SERVER=$BUILD_NO_SOAP_SERVER \
           -DMUDUOX_DIR=$MUDUOX_DIR \
           -DLIBUV_DIR=$LIBUV_DIR \
           -DLIBRE_DIR=$LIBRE_DIR \
           -DTINYXML2_DIR=$TINYXML2_DIR \
           $SOURCE_DIR \
  && make $*

# Use the following command to run all the unit tests
# at the dir $BUILD_DIR/$BUILD_TYPE :
# CTEST_OUTPUT_ON_FAILURE=TRUE make test

# cd $SOURCE_DIR && doxygen

