CFLAGS=-O2 -g
OBJS=closure.o web.o tree.o count.o session.o
LIBS=-la -ltsdb -lace

web: ${OBJS}
	gcc ${CFLAGS} ${OBJS} -o web ${LIBS}

clean:
	rm -f ${OBJS}
