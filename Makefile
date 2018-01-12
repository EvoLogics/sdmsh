PROJ = sdmsh

SRC = $(PROJ).c shell.c sdmsh_commands.c
OBJ = $(SRC:.c=.o)

LIB	= libsdm
LIB_DIR	= lib/$(LIB)
LIB_SO	= $(LIB_DIR)/$(LIB).so
LIB_A	= $(LIB_DIR)/$(LIB).a

CFLAGS = -W -Wall -I. -I$(LIB_DIR) -lreadline -lm -ggdb -DLOGGER_ENABLED -fPIC -fpack-struct

build: lib $(OBJ)
	$(CC) -o $(PROJ) $(OBJ) $(LIB_A) -L. -lreadline

build-dyn: lib $(OBJ)
	$(CC) -o $(PROJ) $(OBJ) -L$(LIB_DIR) -I$(LIB_DIR) -lreadline -lsdm 

.PHONY: lib
lib:
	make -C $(LIB_DIR)

$(LIB_A) $(LIB_SO):
	make -C $(LIB_DIR)

clean:
	make -C $(LIB_DIR) clean
	rm -f $(PROJ) $(OBJ) *~ .*.sw? *.so core

dist-clean: clean
	rm -f cscope.out tags
