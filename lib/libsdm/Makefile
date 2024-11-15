PROJ = libsdm

SRC = sdm.c utils.c janus/janus.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Wall -Wextra -I. -I../libstream -L../libstream -lstream -lm -ggdb -DLOGGER_ENABLED -fPIC

$(PROJ): $(PROJ).so $(PROJ).a

$(PROJ).so: $(OBJ)
	$(CC) -shared $(CFLAGS) -o $@ $^

$(PROJ).a: $(OBJ)
	$(AR) rcs $@ $^

clean:
	rm -f $(PROJ).so $(PROJ).a $(OBJ) *~ .*.sw?
