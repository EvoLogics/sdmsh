PROJ = sdm
LIBSDM_DIR  = ../libsdm
PYTHON_INC = /usr/include/python2.7

TEST_NODE = 131

.PHONY: build
build: $(LIBSDM_DIR)/libsdm.so $(PROJ)_wrap.c _$(PROJ).so

$(PROJ)_wrap.c:
	swig -python -I$(LIBSDM_DIR) $(PROJ).i

_$(PROJ).so:
	$(CC) -DLOGGER_ENABLED -I$(LIBSDM_DIR) -L$(LIBSDM_DIR) -I$(PYTHON_INC) -shared -fPIC -l$(PROJ) -o $@ $(PROJ)_wrap.c

$(LIBSDM_DIR)/libsdm.so:
	make -C $(LIBSDM_DIR)

test: build
	LD_LIBRARY_PATH=$(LIBSDM_DIR) ./test.py $(TEST_NODE)

clean:
	rm -f $(PROJ).py $(PROJ)_wrap.* $(OBJ) *~ *.pyc .*.sw? *.so
