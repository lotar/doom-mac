# DOOM-Mac — original C engine build
CC      := clang
SRCDIR  := src
BINDIR  := bin
DISTDIR := dist
INCDIR  := /opt/homebrew/include

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

SRC := $(wildcard $(SRCDIR)/*.c)
OBJ := $(SRC:.c=.o)

CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -I$(INCDIR) \
           $(SDL_CFLAGS) -DVERSION=\"1.0.0\"
LIBS    := $(SDL_LIBS) -lm

DEBUG_FLAGS := -std=c11 -O1 -g \
               -I$(INCDIR) $(SDL_CFLAGS) -DVERSION=\"1.0.0-debug\"

.PHONY: all debug test clean dist run shots

all: $(BINDIR)/doom

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/doom: $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# AddressSanitizer debug binary + headless tests
$(BINDIR)/doom-debug: $(SRC) | $(BINDIR)
	$(CC) $(DEBUG_FLAGS) -o $@ $(SRC) $(LIBS)

debug: $(BINDIR)/doom-debug

test: $(BINDIR)/doom-debug
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 \
		./$(BINDIR)/doom-debug --selftest

run: $(BINDIR)/doom
	./$(BINDIR)/doom

shots: $(BINDIR)/doom
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./$(BINDIR)/doom --shots shots

# macOS app bundle + zip + dmg
dist: all
	./scripts/package_app.sh

clean:
	rm -rf $(BINDIR) $(DISTDIR) build *.dmg
