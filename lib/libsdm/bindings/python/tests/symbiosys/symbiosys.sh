#!/bin/sh
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-../../../..:../../../../../libstream}
export PYTHONPATH=${PYTHONPATH:-../..}

# [Inferior 1 (process 85751) exited normally
gdb -return-child-result -ex run -ex "thread apply all bt" -ex quit --args python symbiosys.py $@
#./symbiosys.py $@
