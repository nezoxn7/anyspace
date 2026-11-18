#!/usr/bin/sh

rm -rf build
mkdir -p build
cp src/defaultCfg/config.cfg build/config.cfg
make test
