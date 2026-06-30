#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

ktx create $1 $2.ktx --format R8G8B8A8_SRGB --assign-tf srgb --generate-mipmap