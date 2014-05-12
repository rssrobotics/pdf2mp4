PKVID_OBJS = image_u8x3.o pkvid.o string_util.o zarray.o pnm.o generic_tokenizer.o freepool.o zhash.o scheme.o

VIDSCRIPT_OBJS = vidscript.o image_u8x3.o string_util.o zarray.o pnm.o generic_tokenizer.o freepool.o zhash.o scheme.o

VIDSCHEME_OBJS = vidscheme.o image_u8x3.o string_util.o zarray.o pnm.o generic_tokenizer.o freepool.o zhash.o scheme.o

SCHEME_TEST_OBJS = scheme_test.o string_util.o zarray.o pnm.o generic_tokenizer.o freepool.o zhash.o scheme.o

QUEUEWORKER_OBJS = queueworker.o getopt.o zhash.o zarray.o string_util.o

all: scheme_test vidscheme queueworker


queueworker: $(QUEUEWORKER_OBJS)
	gcc -o queueworker $(QUEUEWORKER_OBJS)

vidscheme: $(VIDSCHEME_OBJS)
	gcc -o vidscheme $(VIDSCHEME_OBJS) -lm `pkg-config --libs libpng`

scheme_test: $(SCHEME_TEST_OBJS)
	gcc -o scheme_test $(SCHEME_TEST_OBJS) -lm

vidscript: $(VIDSCRIPT_OBJS)
	gcc -o vidscript $(VIDSCRIPT_OBJS) -lm

pkvid: $(PKVID_OBJS)
	gcc -o pkvid $(PKVID_OBJS) -lm

%.o: %.c
	gcc -c -std=gnu99 -Wall -g $<

clean:
	rm -f *.o *~
