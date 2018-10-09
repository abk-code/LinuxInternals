******************************
rwlock design details using C 
******************************


Few of the ideas have been taken from the following link:
https://stackoverflow.com/questions/12033188/how-would-you-implement-your-own-reader-writer-lock-in-c11

I'm used to kernel's mutex and conditional variable (CV) for doing rwlock.  I
believe pthread_cond_* APIs do the similar job for userspace. The following is
sample psuedo-code of rwlock.

From the link above, rough algorithm is as below.
#################################################

read_lock() {
  mutex.lock();
  while (writer)
    unlocked.wait(mutex);
  readers++;
  mutex.unlock();
}

read_unlock() {
  mutex.lock();
  readers--;
  if (readers == 0)
    unlocked.signal_all();
  mutex.unlock();
}

write_lock() {
  mutex.lock();
  while (writer || (readers > 0))
    unlocked.wait(mutex);
  writer = true;
  mutex.unlock();
}

write_unlock() {
  mutex.lock();
  writer = false;
  unlocked.signal_all();
  mutex.unlock();
}

But this suffers from having no weight for writers. (NO write bias).  So I
slightly modified the algorithm to use 2 CVs (read and write) instead of only
one.
