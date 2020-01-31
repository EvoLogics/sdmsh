PROJ = sdm
LIBSDM_DIR     = ../libsdm
LIBSTREAM_DIR  = ../libstream
LIBS  = -L$(LIBSDM_DIR) -I$(LIBSDM_DIR) -L$(LIBSTREAM_DIR) -I$(LIBSTREAM_DIR)
PYTHON_INC = /usr/include/python2.7

TEST_NODE_rx = 162
TEST_NODE_tx = 171

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

rx tx: build
	LD_LIBRARY_PATH=$(LIBSDM_DIR):$(LIBSTREAM_DIR) ./$@_test.py $(TEST_NODE_$@)
	#LD_LIBRARY_PATH=$(LIBSDM_DIR):$(LIBSTREAM_DIR) valgrind --tool=memcheck --suppressions=valgrind-python.supp ./$@_test.py $(TEST_NODE_$@)

clean:
	rm -f $(PROJ).py $(PROJ)_wrap.* $(OBJ) *~ *.pyc .*.sw? *.so
