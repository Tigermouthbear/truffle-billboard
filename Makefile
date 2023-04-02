SRC=main.c mongoose.c
OBJ=$(SRC:%.c=%.o)

CFLAGS=-g -Werror -I.

.c.o:
	$(CC) -c $(CFLAGS) $<

billboard: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -rf billboards $(OBJ)

.DEFAULT_GOAL: billboard
.PHONY: clean
