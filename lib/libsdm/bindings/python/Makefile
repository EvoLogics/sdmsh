PROJ = sdm

LIBSDM_DIR     = ../..
LIBSTREAM_DIR  = ../../../libstream
LIBS  = -L$(LIBSDM_DIR) -I$(LIBSDM_DIR) -L$(LIBSTREAM_DIR) -I$(LIBSTREAM_DIR)
TESTS_BASE = tests
TEST = usbl

WRAP_SRC  = $(PROJ)_wrap.c

OUT_SO = _$(PROJ).so
SWIG_OPT  = -python -globals var
BIND_LANG_INC = /usr/include/python3.11

OUT_SO = $(PROJ).so
#BIND_LANG_INC = /usr/include/tcl8.6/
#SWIG_OPT  = -tcl8
#SWIG_OPT += -namespace
#SWIG_OPT += -itcl

CC = gcc

TEST_NODE_active = 219
TEST_NODE_passive = 221

.PHONY: build
build: lib $(LIBSDM_DIR)/libsdm.so $(LIBSTREAM_DIR)/libstream.so $(OUT_SO)

$(WRAP_SRC): $(PROJ).i
	#swig -python -E -I$(LIBSDM_DIR) $(PROJ).i
	#swig -python -debug-tmsearch -I$(LIBSDM_DIR) $(PROJ).i
	swig -v -Wall $(SWIG_OPT) -I$(LIBSDM_DIR) -I$(LIBSTREAM_DIR) $(PROJ).i

$(OUT_SO): $(WRAP_SRC)
	$(CC) $(LIBS) -I$(BIND_LANG_INC) -shared -fPIC -l$(PROJ) -o $@ $(WRAP_SRC) \
		-lsdm -lstream

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

devshell:
	LD_LIBRARY_PATH=$(LIBSDM_DIR):$(LIBSTREAM_DIR) PYTHONPATH=. bash -il

clean:
	rm -f $(PROJ).py $(PROJ)_wrap.* $(OBJ) *~ *.pyc .*.sw? *.so
