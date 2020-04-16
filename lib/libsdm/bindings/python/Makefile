PROJ = sdm
LIBSDM_DIR     = ../..
LIBSTREAM_DIR  = ../../../libstream
LIBS  = -L$(LIBSDM_DIR) -I$(LIBSDM_DIR) -L$(LIBSTREAM_DIR) -I$(LIBSTREAM_DIR)
PYTHON_INC = /usr/include/python2.7
TESTS_BASE = tests
TEST = usbl

CC = gcc

TEST_NODE_active = 219
TEST_NODE_passive = 221

.PHONY: build
build: lib $(LIBSDM_DIR)/libsdm.so $(LIBSTREAM_DIR)/libstream.so _$(PROJ).so

$(PROJ)_wrap.c: $(PROJ).i
	#swig -python -E -I$(LIBSDM_DIR) $(PROJ).i
	#swig -python -debug-tmsearch -I$(LIBSDM_DIR) $(PROJ).i
	swig -v -python -I$(LIBSDM_DIR) -I$(LIBSTREAM_DIR) $(PROJ).i

_$(PROJ).so: $(PROJ)_wrap.c
	$(CC) -DLOGGER_ENABLED $(LIBS) -I$(PYTHON_INC) -shared -fPIC -l$(PROJ) -o $@ $(PROJ)_wrap.c

.PHONY: lib
lib:
	make -C $(LIBSDM_DIR) && make -C $(LIBSTREAM_DIR)

$(LIBSDM_DIR)/libsdm.so:
	make -C $(LIBSDM_DIR)

$(LIBSTREAM_DIR)/libstream.so:
	make -C $(LIBSTREAM_DIR)

active passive: build
	LD_LIBRARY_PATH=$(LIBSDM_DIR):$(LIBSTREAM_DIR) PYTHONPATH=. tests/$(TEST)/$(TEST)-$@.py $(TEST_NODE_$@)
	#LD_LIBRARY_PATH=$(LIBSDM_DIR):$(LIBSTREAM_DIR) PYTHONPATH=. valgrind --tool=memcheck --suppressions=valgrind-python.supp tests/$(TEST)/$(TEST)-$@.py $(TEST_NODE_$@)

clean:
	rm -f $(PROJ).py $(PROJ)_wrap.* $(OBJ) *~ *.pyc .*.sw? *.so
