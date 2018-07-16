#!/bin/sh -x

if [ -z "$1" ]; then
    echo $0 file-s16.raw >&2
    exit 1
fi

if ! which gnuplot > /dev/null; then
    echo gnuplot need to be installed >&2
    exit 1
fi

# set terminal svg;
# set output 'test.png';

if false; then
    gnuplot -p -e "set multiplot layout 3,1 ; \
    plot 0 ls 2, '$1' binary format='%int16' using 0:1 with lines ls 1; \
    plot 0 ls 2, '$2' binary format='%int16' using 0:2 with lines ls 1; \
    unset multiplot"
    exit
fi

gnuplot -p -e "plot '$1' binary format='%int16' using 0:1 with lines;"

