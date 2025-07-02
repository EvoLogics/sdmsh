# Uncomment this if you have readline version 6
#COMPAT_READLINE6 = 1

# Uncomment this if you want static binary
#BUILD_STATIC_BIN = 1

# Comment this if you do not want checking with address sanitize
# NOTE: you can use shell variable ASAN_OPTIONS=fast_unwind_on_malloc=false before run sdmsh
# also read https://github.com/google/sanitizers/wiki/AddressSanitizerFlags#run-time-flags
#WITH_ADDRESS_SANITAZE = 1
##############################
PROJ = sdmsh

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

ifdef BUILD_STATIC_BIN
    STATIC = --static
endif

RLINC != pkg-config --cflags readline 2> /dev/null
RLLIB != pkg-config --libs ${STATIC} readline 2> /dev/null

CFLAGS += -Wall -Wextra -I. -I$(LIBSDM_DIR) -I$(LIBSTRM_DIR) -ggdb -DLOGGER_ENABLED -D_GNU_SOURCE -fPIC ${RLINC}
ifdef WITH_ADDRESS_SANITAZE
	CFLAGS  += -fsanitize=address
	LDFLAGS += -fsanitize=address
endif

ifeq ($(RLLIB),)
   ifdef BUILD_STATIC_BIN
      LDFLAGS += -l:libreadline.a
   else
      LDFLAGS += -lreadline
   endif
endif

ifdef BUILD_STATIC_BIN
    LDFLAGS += -static -l:libtinfo.a -l:libncurses.a
else
    LDFLAGS += -ltinfo -lncurses
endif

LDFLAGS += ${RLLIB} -lm

ifdef COMPAT_READLINE6
    SRC     +=  compat/readline6.c
    CFLAGS  += -DCOMPAT_READLINE6
endif

DOCKER_RUN = docker run --rm -it \
                -w $(PWD) -v $(PWD):$(PWD) \
                -v $(HOME)/.bash_history:$(HOME)/.bash_history \
                -e USER=$(USER) -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
                evologicsgmbh/crosscompile-sandbox

all: build sampleconv

build: lib $(OBJ)
	$(CC) -o $(PROJ) $(OBJ) $(LIBSDM_A) $(LIBSTRM_A) -L. $(LDFLAGS)

build-dyn: lib $(OBJ)
	$(CC) $(LDFLAGS) -o $(PROJ) $(OBJ) -L$(LIBSDM_DIR) -I$(LIBSDM_DIR) -L$(LIBSTRM_DIR) -I$(LIBSTRM_DIR) -lsdm

sandbox-build:
	$(DOCKER_RUN) make

sandbox-devshell:
	$(DOCKER_RUN)

.PHONY: lib
lib:
	${MAKE} -C $(LIBSTRM_DIR)
	${MAKE} -C $(LIBSDM_DIR)

$(LIBSDM_A) $(LIBSDM_SO):
	${MAKE} -C $(LIBSTRM_DIR)
	${MAKE} -C $(LIBSDM_DIR)

clean:
	${MAKE} -C $(LIBSDM_DIR) clean
	${MAKE} -C $(LIBSTRM_DIR) clean
	rm -f $(PROJ) $(OBJ) *~ .*.sw? *.so core *.core

dist-clean: clean
	rm -f cscope.out tags
