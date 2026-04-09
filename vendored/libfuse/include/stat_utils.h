#pragma once

#if defined(__linux__) || defined(_WIN32)
/* Our Windows compat struct stat has st_atim/st_mtim/st_ctim too */
#define ST_ATIM_NSEC(ST) ((ST)->st_atim.tv_nsec)
#define ST_CTIM_NSEC(ST) ((ST)->st_ctim.tv_nsec)
#define ST_MTIM_NSEC(ST) ((ST)->st_mtim.tv_nsec)
#elif defined __FreeBSD__
#define ST_ATIM_NSEC(ST) ((ST)->st_atimespec.tv_nsec)
#define ST_CTIM_NSEC(ST) ((ST)->st_ctimespec.tv_nsec)
#define ST_MTIM_NSEC(ST) ((ST)->st_mtimespec.tv_nsec)
#else
#error "Unsupported platform for st_{a,c,m}time nanosecond"
#endif
