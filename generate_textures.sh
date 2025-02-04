#! /bin/sh

generate () {
    TYPE="$1"
    SIZE="$2"
    NAME="${TYPE}_${SIZE}x${SIZE}"

    echo "Generate '${NAME}'"
    "${REL_PATH}/build/noise" -t "${TYPE}" -s ${SIZE} -f pgm -o "${REL_PATH}/build/${NAME}.pgm"
    convert "${REL_PATH}/build/${NAME}.pgm" "${REL_PATH}/textures/${NAME}.png"
    optipng "${REL_PATH}/textures/${NAME}.png" 2> /dev/null
    rm "${REL_PATH}/build/${NAME}.pgm"
}

REL_PATH=$(dirname "$0")

if ! [ -x "$(command -v convert)" ]; then
    echo "'convert' was not found. Please install it. This is part of ImageMagick"
    exit 1
fi

if ! [ -x "$(command -v optipng)" ]; then
    echo "'optipng' was not found. Please install it."
    exit 1
fi

if [ ! -d "${REL_PATH}/textures" ]; then
    mkdir "${REL_PATH}/textures"
fi

generate "dither-blue" 64
generate "dither-bayer" 2
generate "dither-bayer" 4
generate "dither-bayer" 8
generate "dither-bayer" 16
