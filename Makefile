PROJ = sdmsh

LIB_SRC = sdm.c logger.c
LIB_OBJ = $(LIB_SRC:.c=.o)

SRC = $(PROJ).c shell.c commands.c
OBJ = $(SRC:.c=.o)

CFLAGS = -W -Wall -I. -lreadline -lm -ggdb -DLOGGER_ENABLED -fPIC -fpack-struct

no-sdm-so: $(LIB_OBJ) $(OBJ)
	$(CC) -L. -lreadline -o $(PROJ) $(OBJ) $(LIB_OBJ)

binary: lib $(OBJ)
	$(CC) -L. -I. -lreadline -lsdm -o $(PROJ) $(OBJ)

lib: $(LIB_OBJ)
	$(CC) -shared -DLOGGER_ENABLED -o libsdm.so $^

clean:
	rm -f $(PROJ) $(OBJ) $(LIB_OBJ) *~ .*.sw? *.so

dist-clean: clean
	rm -f cscope.out tags
