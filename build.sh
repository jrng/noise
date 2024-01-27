#! /bin/sh

COMPILER="clang"
COMPILER_FLAGS="-std=c99 -g"
LINKER_FLAGS="-lm"

compile () {
    echo "$1"
    $1
}

REL_PATH=$(dirname "$0")

if [ ! -d "${REL_PATH}/build" ]; then
    mkdir "${REL_PATH}/build"
fi

cd "${REL_PATH}/build"

compile "${COMPILER} ${COMPILER_FLAGS} -o noise ../src/main.c ${LINKER_FLAGS}"
