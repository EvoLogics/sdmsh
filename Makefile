PROJ = sdmsh

SRC = $(PROJ).c shell.c commands.c
OBJ = $(SRC:.c=.o)

LIB	= libsdm
LIB_DIR	= lib/$(LIB)
LIB_SO	= $(LIB_DIR)/$(LIB).so
LIB_A	= $(LIB_DIR)/$(LIB).a

CFLAGS = -W -Wall -I. -I$(LIB_DIR) -lreadline -lm -ggdb -DLOGGER_ENABLED -fPIC -fpack-struct

build: lib $(OBJ)
	$(CC) -L. -lreadline -o $(PROJ) $(OBJ) $(LIB_A)

build-dyn: lib $(OBJ)
	$(CC) -L$(LIB_DIR) -I$(LIB_DIR) -lreadline -lsdm -o $(PROJ) $(OBJ)

.PHONY: lib
lib:
	make -C $(LIB_DIR)

$(LIB_A) $(LIB_SO):
	make -C $(LIB_DIR)

clean:
	make -C $(LIB_DIR) clean
	rm -f $(PROJ) $(OBJ) *~ .*.sw? *.so

dist-clean: clean
	rm -f cscope.out tags
