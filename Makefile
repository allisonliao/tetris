CC      = cc
CFLAGS  = -O2 -Wall

BINS    = tetris_model tetris_display

.PHONY: all clean run

all: $(BINS)

tetris_model: tetris_model.c
	$(CC) $(CFLAGS) -o $@ $<

tetris_display: tetris_display.c
	$(CC) $(CFLAGS) -o $@ $<

run: tetris_model
	./tetris_model
