#!/bin/bash

# make config=development_linux clean
# bear -- make config=development_linux
# -checks="modernize-use-override",bugprone,concurrency
clang-tidy -p Build Src/*.cpp Src/*.h -config-file=.clang-tidy