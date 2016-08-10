#!/bin/sh

mkdir -p build
cd build
qmake ../xastir-qt.pro
make

