CC = gcc
INC=xv6-riscv/kernel
CFLAGS = -Wall -Werror -pedantic -ggdb -O0
OBJS = xcheck.o xtest.o

.SUFFIXES: .c .o 

all: xcheck

xcheck: xcheck.o $(INC)/fs.h $(INC)/stat.h $(INC)/param.h $(INC)/types.h 
	$(CC) $(CFLAGS) -o xcheck xcheck.o

xtest: xtest.o $(INC)/fs.h $(INC)/stat.h $(INC)/param.h $(INC)/types.h
	$(CC) $(CFLAGS) -o xtest xtest.o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) xcheck xtest testfs*

