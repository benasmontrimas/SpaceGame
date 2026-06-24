#!/bin/bash

clang-format -i Src/*.cpp Src/*.h
clang-format -i Assets/Shaders/*.slang --style="file:.clang-format-shaders"