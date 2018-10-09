CFLAGS=-pthread -g -c

SOURCES=rwlock.c

rwlock.o: rwlock.c rwlock.h errors.h
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ rwlock.c

clean:
	@rm -rf $(PROGRAMS) *.o

recompile:  clean all
