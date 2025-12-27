/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#include <stddef.h>
#if HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif // HAVE_SYS_TIME_H
#include <time.h>

#include "xa_config.h"
#include "mutex_utils.h"

// For mutex debugging with Linux threads only
#ifdef MUTEX_DEBUG
  #include <asm/errno.h>
  //
  // Newer pthread function
  #ifdef HAVE_PTHREAD_MUTEXATTR_SETTYPE
    extern int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);
  #endif  // HAVE_PTHREAD_MUTEXATTR_SETTYPE
  //
  // Older, deprecated pthread function
  #ifdef HAVE_PTHREAD_MUTEXATTR_SETKIND_NP
    extern int pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind);
  #endif  // HAVE_PTHREAD_MUTEXATTR_SETKIND_NP
  //
#endif  // MUTEX_DEBUG


//----------------------------------------------------------------------
// Implements safer mutex locking.  Posix-compatible mutex's will lock
// up a thread if lock's/unlock's aren't in perfect pairs.  They also
// allow one thread to unlock a mutex that was locked by a different
// thread.
// Remember to keep this thread-safe.  Need dynamic storage (no statics)
// and thread-safe calls.
//
// In order to keep track of process ID's in a portable way, we probably
// can't use the process ID stored inside the pthread_mutex_t struct.
// There's no easy way to get at it.  For that reason we'll create
// another struct that contains both the process ID and a pointer to
// the pthread_mutex_t, and pass pointers to those structs around in
// our programs instead.  The new struct is "xastir_mutex".
//


// Function to initialize the new xastir_mutex
// objects before use.
//
void init_critical_section(xastir_mutex *lock)
{
#ifdef MUTEX_DEBUG
  pthread_mutexattr_t attr;

  // Note that this stuff is not POSIX, so is considered non-portable.
  // Exists in Linux Threads implementation only?

  // Check first for newer pthread function
#  ifdef HAVE_PTHREAD_MUTEXATTR_SETTYPE
  (void)pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#  else
  // Use older, deprecated pthread function
  (void)pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
#  endif  // HAVE_PTHREAD_MUTEXATTR_SETTYPE

  (void)pthread_mutexattr_init(&attr);
  (void)pthread_mutex_init(&lock->lock, &attr);
#else   // MUTEX_DEBUG
  (void)pthread_mutex_init(&lock->lock, NULL); // Set to default POSIX-compatible type
#endif  // MUTEX_DEBUG

  lock->threadID = 0;  // No thread has locked it yet
}





// Function which uses xastir_mutex objects to lock a
// critical shared section of code or variables.  Makes
// sure that only one thread can access the critical section
// at a time.  If there are no problems, it returns zero.
//
int begin_critical_section(xastir_mutex *lock, char *text)
{
  pthread_t calling_thread;
  int problems;
#ifdef MUTEX_DEBUG
  int ret;
#endif  // MUTEX_DEBUG

  problems = 0;

  // Note that /usr/include/bits/pthreadtypes.h has the
  // definition for pthread_mutex_t, and it includes the
  // owner thread number:  _pthread_descr pthread_mutex_t.__m_owner
  // It's probably not portable to use it though.

  // Get thread number we're currently running under
  calling_thread = pthread_self();

  if (pthread_equal( lock->threadID, calling_thread ))
  {
    // We tried to lock something that we already have the lock on.
    fprintf(stderr,"%s:Warning:This thread already has the lock on this resource\n", text);
    problems++;
    return(0);  // Return the OK code.  Skip trying the lock.
  }

  //if (lock->threadID != 0)
  //{
  // We tried to lock something that another thread has the lock on.
  // This is normal operation.  The pthread_mutex_lock call below
  // will put our process to sleep until the mutex is unlocked.
  //}

  // Try to lock it.
#ifdef MUTEX_DEBUG
  // Instead of blocking the thread, we'll loop here until there's
  // no deadlock, printing out status as we go.
  do
  {
    ret = pthread_mutex_lock(&lock->lock);
    if (ret == EDEADLK)     // Normal operation.  At least two threads want this lock.
    {
      fprintf(stderr,"%s:pthread_mutex_lock(&lock->lock) == EDEADLK\n", text);
      sched_yield();  // Yield the cpu to another thread
    }
    else if (ret == EINVAL)
    {
      fprintf(stderr,"%s:pthread_mutex_lock(&lock->lock) == EINVAL\n", text);
      fprintf(stderr,"Forgot to initialize the mutex object...\n");
      problems++;
      sched_yield();  // Yield the cpu to another thread
    }
  }
  while (ret == EDEADLK);

  if (problems)
  {
    fprintf(stderr,"Returning %d to calling thread\n", problems);
  }
#else   // MUTEX_DEBUG
  pthread_mutex_lock(&lock->lock);
#endif  // MUTEX_DEBUG

  // Note:  This can only be set AFTER we already have the mutex lock.
  // Otherwise there may be contention with other threads for setting
  // this variable.
  //
  lock->threadID = calling_thread;    // Save the threadID that we used
  // to lock this resource

  return(problems);
}





// Function which ends the locking of a critical section
// of code.  If there are no problems, it returns zero.
//
int end_critical_section(xastir_mutex *lock, char *text)
{
  pthread_t calling_thread;
  int problems;
#ifdef MUTEX_DEBUG
  int ret;
#endif  // MUTEX_DEBUG

  problems = 0;

  // Get thread number we're currently running under
  calling_thread = pthread_self();

  if (lock->threadID == 0)
  {
    // We have a problem.  This resource hasn't been locked.
    fprintf(stderr,"%s:Warning:Trying to unlock a resource that hasn't been locked:%ld\n",
            text,
            (long int)lock->threadID);
    problems++;
  }

  if (! pthread_equal( lock->threadID, calling_thread ))
  {
    // We have a problem.  We just tried to unlock a mutex which
    // a different thread originally locked.  Not good.
    fprintf(stderr,"%s:Trying to unlock a resource that a different thread locked originally: %ld:%ld\n",
            text,
            (long int)lock->threadID,
            (long int)calling_thread);
    problems++;
  }


  // We need to clear this variable BEFORE we unlock the mutex, 'cuz
  // other threads might be waiting to lock the mutex.
  //
  lock->threadID = 0; // We're done with this lock.


  // Try to unlock it.  Compare the thread identifier we used before to make
  // sure we should.

#ifdef MUTEX_DEBUG
  // EPERM error: We're trying to unlock something that we don't have a lock on.
  ret = pthread_mutex_unlock(&lock->lock);

  if (ret == EPERM)
  {
    fprintf(stderr,"%s:pthread_mutex_unlock(&lock->lock) == EPERM\n", text);
    problems++;
  }
  else if (ret == EINVAL)
  {
    fprintf(stderr,"%s:pthread_mutex_unlock(&lock->lock) == EINVAL\n", text);
    fprintf(stderr,"Someone forgot to initialize the mutex object\n");
    problems++;
  }

  if (problems)
  {
    fprintf(stderr,"Returning %d to calling thread\n", problems);
  }
#else   // MUTEX_DEBUG
  (void)pthread_mutex_unlock(&lock->lock);
#endif  // MUTEX_DEBUG

  return(problems);
}
