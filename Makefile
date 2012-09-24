CFLAGS=-O2 -g
CC=gcc
#CFLAGS=-g

OBJS=closure.o web.o tree.o count.o session.o reconstruct.o forest.o
#LIBS=-la -ltsdb -lace
DELPHIN_LIBS=-L ${LOGONROOT}/lingo/lkb/lib/linux.x86.64 -Wl,-Bstatic -litsdb -lpvm3 -Wl,-Bdynamic
LIBS=-Wl,-Bstatic -la -ltsdb -lace -lrepp -lboost_regex -lstdc++ -lm -lutil -Wl,-Bdynamic -ldl -lpthread ${DELPHIN_LIBS} -z muldefs

web: ${OBJS}
	gcc ${CFLAGS} ${OBJS} -o web ${LIBS}

clean:
	rm -f ${OBJS}
