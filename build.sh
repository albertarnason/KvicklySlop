#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

#########
# Flags #
#########

# i dont really care about most of these except making the compiler relatively strict
COMMON_FLAGS="-std=c++11 -Wall -Werror -Wno-unused -Wshadow -Wconversion -Wsign-conversion -Wundef -Wformat=2 -fno-exceptions -fno-rtti -I SlopEngine/engine"

DEBUG_FLAGS="-O0 -g -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
RELEASE_FLAGS="-O2 -DHANDMADE_INTERNAL=0 -DHANDMADE_SLOW=0"

MODE="debug"

if [[ "${1:-}" == "-r" || "${1:-}" == "--release" ]]; then
  MODE="release"
fi

if [[ "$MODE" == "release" ]]; then
  BUILD_FLAGS="$RELEASE_FLAGS"
else
  BUILD_FLAGS="$DEBUG_FLAGS"
fi

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    PLATFORM_FLAGS="$(pkg-config --cflags --libs sdl3)"
else
    PLATFORM_FLAGS="-ldl $(pkg-config --cflags --libs sdl3)"
fi

##############
# Game layer #
##############

g++ SlopEngine/engine/handmade.cpp -o build/handmade.so -shared -fPIC $COMMON_FLAGS $BUILD_FLAGS

##################
# Platform layer #
##################

g++ SlopEngine/platform/sdl_platform.cpp -o build/handmade $COMMON_FLAGS $BUILD_FLAGS $PLATFORM_FLAGS

