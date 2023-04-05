LIBS=$(shell pkg-config --cflags --libs hiredis libcurl)
SRC=billboard.c mongoose.c
OBJ=$(SRC:%.c=%.o)

CFLAGS=-g -Werror -I.

.c.o:
	$(CC) -c $(CFLAGS) $< $(LIBS)

billboard: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -rf billboard $(OBJ)

.DEFAULT_GOAL: billboard
.PHONY: clean
