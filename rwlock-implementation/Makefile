CFLAGS=-pthread -g 

PROGRAMS=rwlock_main
all:  ${PROGRAMS}

rwlock_main: rwlock.c rwlock.h rwlock_main.c
	${CC} ${CFLAGS} -o $@ rwlock_main.c rwlock.c

clean:
	@rm -rf $(PROGRAMS) *.o

recompile:  clean all
