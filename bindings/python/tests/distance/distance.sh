#!/bin/sh
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-../../../..:../../../../../libstream}
export PYTHONPATH=${PYTHONPATH:-../..}
#valgrind --tool=memcheck --suppressions=../../../../../libsdm/bindings/python/valgrind-python.supp ./distance.py $@
./distance.py $@
