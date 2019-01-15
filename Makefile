PROJ = sdmsh

# Uncomment this if you have readline version 6
#COMPAT_READLINE6 = 1

SRC = $(PROJ).c shell.c sdmsh_commands.c shell_history.c shell_completion.c shell_help.c
OBJ = $(SRC:.c=.o)

LIBSDM	   = libsdm
LIBSDM_DIR = lib/$(LIBSDM)
LIBSDM_SO  = $(LIBSDM_DIR)/$(LIBSDM).so
LIBSDM_A   = $(LIBSDM_DIR)/$(LIBSDM).a

LIBSTRM	    = libstream
LIBSTRM_DIR = lib/$(LIBSTRM)
LIBSTRM_SO  = $(LIBSTRM_DIR)/$(LIBSTRM).so
LIBSTRM_A   = $(LIBSTRM_DIR)/$(LIBSTRM).a

CFLAGS = -W -Wall -I. -I$(LIBSDM_DIR) -I$(LIBSTRM_DIR) -ggdb -DLOGGER_ENABLED -fPIC -fpack-struct

ifneq ($(COMPAT_READLINE6),)
SRC     +=  compat/readline6.c
CFLAGS  += -DCOMPAT_READLINE6
LDFLAGS += -lncurses -ltinfo
endif

build: lib $(OBJ)
	$(CC) $(LDFLAGS) -o $(PROJ) $(OBJ) $(LIBSDM_A) $(LIBSTRM_A) -L. -lreadline

build-dyn: lib $(OBJ)
	$(CC) $(LDFLAGS) -o $(PROJ) $(OBJ) -L$(LIBSDM_DIR) -I$(LIBSDM_DIR) -L$(LIBSTRM_DIR) -I$(LIBSTRM_DIR) -lreadline -lsdm

.PHONY: lib
lib:
	make -C $(LIBSDM_DIR)
	make -C $(LIBSTRM_DIR)

$(LIBSDM_A) $(LIBSDM_SO):
	make -C $(LIBSDM_DIR)
	make -C $(LIBSTRM_DIR)

clean:
	make -C $(LIBSDM_DIR) clean
	make -C $(LIBSTRM_DIR) clean
	rm -f $(PROJ) $(OBJ) *~ .*.sw? *.so core

dist-clean: clean
	rm -f cscope.out tags
