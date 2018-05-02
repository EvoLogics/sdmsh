PROJ = libstream

SRC = stream.c stream_raw.c stream_ascii.c stream_tcp.c
OBJ = $(SRC:.c=.o)

CFLAGS = -W -Wall -I. -lm -ggdb -DLOGGER_ENABLED -fPIC -fpack-struct

$(PROJ): $(PROJ).so $(PROJ).a

$(PROJ).so: $(OBJ)
	$(CC) -shared $(CFLAGS) -o $@ $^

$(PROJ).a: $(OBJ)
	$(AR) rcs $@ $^

clean:
	rm -f $(PROJ).so $(PROJ).a $(OBJ) *~ .*.sw?
