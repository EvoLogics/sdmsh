#!/bin/sh
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-../../../..:../../../../../libstream}
export PYTHONPATH=${PYTHONPATH:-../..}

# Uncoment this for debug in gdb
#gdb -return-child-result -ex run -ex "thread apply all bt" -ex quit --args python symbiosys.py $@

./symbiosys-parallel.py $@
