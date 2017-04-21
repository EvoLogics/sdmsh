PROJ = sdmsh
SRC = $(PROJ).c sdm.c shell.c commands.c logger.c
OBJ = $(SRC:.c=.o)
CFLAGS = -W -Wall -I. -lreadline -lm -ggdb -DLOGGER_ENABLED -c -fPIC

all: $(OBJ)
	$(CC) -o $(PROJ) $(OBJ) -lreadline

lib: sdm.o logger.o
	$(CC) -shared -o sdm.so $^

clean:
	rm -f $(PROJ) $(OBJ) *~ .*.sw?

dist-clean: clean
	rm -f cscope.out tags
