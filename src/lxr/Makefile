stringify: stringify.c
	gcc -o $@ $<

SRC = source.c api.c tokenize.c license.c

stringified.h: stringify $(SRC)
	./stringify $(SRC) > stringified.h
 
