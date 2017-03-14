PROJ = sdmsh
SRC = $(PROJ).c sdm.c shell.c commands.c logger.c
OBJ = $(SRC:.c=.o)
CFLAGS = -W -Wall -I. -lreadline -lm -ggdb -DLOGGER_ENABLED

all: $(OBJ)
	$(CC) -o $(PROJ) $(OBJ) $(CFLAGS)

clean:
	rm -f $(PROJ) $(OBJ) *~ .*.sw?

dist-clean: clean
	rm -f cscope.out tags
