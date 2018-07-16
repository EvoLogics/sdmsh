#!/bin/sh -e

die() {
    echo "$*" >&2
    exit 1
}

[ -z "$1" ] && die "$(basename $0) file.wav"

WAV="$1"
RAW="$(basename "$WAV" .wav).raw"

sox -V4 "$WAV" -b 16 "$RAW"

