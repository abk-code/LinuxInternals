/*
 * rwlock.h
 *
 * This header file describes the "reader/writer lock" synchronization
 * construct.
 *
 * A reader/writer lock allows a thread to lock shared data either for shared
 * read access or exclusive write access.
 *
 * The rwlock_init() and rwlock_uninit() functions, respectively, allow you to
 * initialize/create and destroy/free the reader/writer lock.
 */
#include <pthread.h>

/*
 * Structure describing a read-write lock.
 */
typedef struct rwlock_tag {
    pthread_mutex_t     mutex;
    pthread_cond_t      read;           /* wait for read */
    pthread_cond_t      write;          /* wait for write */
    int                 valid;          /* set when valid */
    int                 r_active;       /* readers active */
    int                 w_active;       /* writer active */
    int                 r_wait;         /* readers waiting */
    int                 w_wait;         /* writers waiting */
} rwlock_t;

#define RWLOCK_VALID    0xfacade

/*
 * Support static initialization of barriers
 */
#define RWL_INITIALIZER \
    {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
    PTHREAD_COND_INITIALIZER, RWLOCK_VALID, 0, 0, 0, 0}

/*
 * Define read-write lock functions
 */
extern int rwlock_init(rwlock_t *rwlock);
extern int rwlock_uninit(rwlock_t *rwlock);
extern int rwlock_lock_rd(rwlock_t *rwlock);
extern int rwlock_unlock_rd(rwlock_t *rwlock);
extern int rwlock_lock_wr(rwlock_t *rwlock);
extern int rwlock_unlock_wr(rwlock_t *rwlock);
