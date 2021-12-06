CC = gcc 
CFLAGS = -Wall -W -Wshadow -std=gnu99 
TARGETS = prog
 
all: $(TARGETS)

prog: prog.o sockwrap.o ip_list.o

clean: 
	rm -f *.o	

distclean: clean
	rm -f $(TARGETS)
