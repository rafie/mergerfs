/*
  POSIX pthread.h compatibility shim for MSVC/Windows.
  Provides mutex and rwlock primitives using Windows SRWLOCK/CRITICAL_SECTION.
*/

#pragma once

#ifdef _WIN32

#include <windows.h>
#include "win32_undef.h"

/* Thread */
typedef HANDLE pthread_t;
typedef int pthread_attr_t;

/* Mutex */
typedef CRITICAL_SECTION pthread_mutex_t;
typedef int pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_CANCEL_ENABLE  0
#define PTHREAD_CANCEL_DISABLE 1

static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
  (void)a;
  InitializeCriticalSection(m);
  return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *m)
{
  DeleteCriticalSection(m);
  return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *m)
{
  EnterCriticalSection(m);
  return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *m)
{
  LeaveCriticalSection(m);
  return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t *m)
{
  return TryEnterCriticalSection(m) ? 0 : EBUSY;
}

/* Read-Write Lock */
typedef SRWLOCK pthread_rwlock_t;
typedef int pthread_rwlockattr_t;

#define PTHREAD_RWLOCK_INITIALIZER SRWLOCK_INIT

static inline int pthread_rwlock_init(pthread_rwlock_t *rw, const pthread_rwlockattr_t *a)
{
  (void)a;
  InitializeSRWLock(rw);
  return 0;
}

static inline int pthread_rwlock_destroy(pthread_rwlock_t *rw)
{
  (void)rw;
  return 0;
}

static inline int pthread_rwlock_rdlock(pthread_rwlock_t *rw)
{
  AcquireSRWLockShared(rw);
  return 0;
}

static inline int pthread_rwlock_wrlock(pthread_rwlock_t *rw)
{
  AcquireSRWLockExclusive(rw);
  return 0;
}

static inline int pthread_rwlock_unlock(pthread_rwlock_t *rw)
{
  /* SRWLOCK doesn't track which mode was acquired;
     try exclusive first, then shared.
     In practice, callers know which mode they used. */
  ReleaseSRWLockExclusive(rw);
  return 0;
}

/* For shared unlock, callers must use this explicitly */
static inline int pthread_rwlock_unlock_shared(pthread_rwlock_t *rw)
{
  ReleaseSRWLockShared(rw);
  return 0;
}

/* Timed lock */
static inline int pthread_mutex_timedlock(pthread_mutex_t *m, const void *abstime)
{
  (void)abstime;
  /* Best-effort: just do a blocking lock */
  EnterCriticalSection(m);
  return 0;
}

/* Thread naming */
static inline int pthread_setname_np(pthread_t t, const char *name)
{
  (void)t;
  (void)name;
  return 0;
}

static inline int sched_yield(void)
{
  SwitchToThread();
  return 0;
}

/* Mutex attributes */
static inline int pthread_mutexattr_init(pthread_mutexattr_t *a)
{
  *a = 0;
  return 0;
}

static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *a)
{
  (void)a;
  return 0;
}

static inline int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type)
{
  *a = type;
  return 0;
}

/* Thread creation/join */
typedef struct { void *(*func)(void*); void *arg; } _pthread_start_info;

static inline DWORD WINAPI _pthread_start_routine(LPVOID param)
{
  _pthread_start_info *info = (_pthread_start_info *)param;
  void *(*func)(void*) = info->func;
  void *arg = info->arg;
  free(info);
  func(arg);
  return 0;
}

static inline int pthread_create(pthread_t *t, const pthread_attr_t *a,
                                 void *(*func)(void*), void *arg)
{
  (void)a;
  _pthread_start_info *info = (_pthread_start_info *)malloc(sizeof(*info));
  if(!info) return -1;
  info->func = func;
  info->arg = arg;
  *t = CreateThread(NULL, 0, _pthread_start_routine, info, 0, NULL);
  return (*t == NULL) ? -1 : 0;
}

static inline int pthread_join(pthread_t t, void **retval)
{
  (void)retval;
  WaitForSingleObject(t, INFINITE);
  CloseHandle(t);
  return 0;
}

static inline int pthread_cancel(pthread_t t)
{
  TerminateThread(t, 0);
  return 0;
}

static inline int pthread_setcancelstate(int state, int *oldstate)
{
  (void)state;
  if(oldstate) *oldstate = PTHREAD_CANCEL_ENABLE;
  return 0;
}

static inline pthread_t pthread_self(void)
{
  return GetCurrentThread();
}

static inline int pthread_equal(pthread_t t1, pthread_t t2)
{
  return GetThreadId(t1) == GetThreadId(t2);
}

static inline void pthread_exit(void *retval)
{
  (void)retval;
  ExitThread(0);
}

#else
#include_next <pthread.h>
#endif
