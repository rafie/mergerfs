/*
  Windows implementations/stubs for vendored libfuse functions.

  On Linux, these are compiled from vendored/libfuse/lib/*.cpp and linked
  into libfuse.a.  On Windows, we skip vendored libfuse entirely and link
  WinFSP instead.  This file provides the mergerfs-specific extensions
  that WinFSP does not supply.
*/

#ifdef _WIN32

#include "fuse_cfg.hpp"
#include "fuse_dirents.hpp"
#include "fuse_dirent.h"
#include "syslog.hpp"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// fuse_cfg — global configuration instance
// (vendored/libfuse/lib/fuse_cfg.cpp)
// ---------------------------------------------------------------------------

fuse_cfg_t fuse_cfg;

bool fuse_cfg_t::valid_uid() const { return (uid != FUSE_CFG_INVALID_ID); }
bool fuse_cfg_t::valid_gid() const { return (gid != FUSE_CFG_INVALID_ID); }
bool fuse_cfg_t::valid_umask() const { return (umask != FUSE_CFG_INVALID_UMASK); }

std::shared_ptr<FILE>
fuse_cfg_t::log_file() const
{
  return std::atomic_load(&_log_file);
}

void
fuse_cfg_t::log_file(std::shared_ptr<FILE> f_)
{
  std::atomic_store(&_log_file, f_);
}

std::shared_ptr<std::string>
fuse_cfg_t::log_filepath() const
{
  return std::atomic_load(&_log_filepath);
}

void
fuse_cfg_t::log_filepath(const std::string &s_)
{
  std::atomic_store(&_log_filepath,
                    std::make_shared<std::string>(s_));
}

// ---------------------------------------------------------------------------
// fuse_debug_set_output
// (vendored/libfuse/lib/debug.cpp — simplified for Windows)
// ---------------------------------------------------------------------------

static
void
_fclose_deleter(FILE *f_)
{
  if(f_ == nullptr || f_ == stdin || f_ == stdout || f_ == stderr)
    return;
  fclose(f_);
}

int
fuse_debug_set_output(const std::string &filepath_)
{
  if(filepath_.empty())
    {
      auto new_log_file = std::shared_ptr<FILE>(stderr,[](FILE*){});
      fuse_cfg.log_file(new_log_file);
      fuse_cfg.log_filepath("");
      return 0;
    }

  FILE *tmp = fopen(filepath_.c_str(),"a");
  if(tmp == NULL)
    return -errno;

  setvbuf(tmp,NULL,_IOLBF,0);

  auto new_log_file = std::shared_ptr<FILE>(tmp,::_fclose_deleter);
  fuse_cfg.log_file(new_log_file);
  fuse_cfg.log_filepath(filepath_);

  return 0;
}


// ---------------------------------------------------------------------------
// fuse_dirents — directory entry buffer management
// (vendored/libfuse/lib/fuse_dirents.cpp — platform-independent)
// ---------------------------------------------------------------------------

#define DENTS_BUF_EXPAND_SIZE        (1024 * 32)
#define DENTS_OFFS_INITIAL_CAPACITY  64

static
uint64_t
_round_up(const uint64_t number_,
          const uint64_t multiple_)
{
  return (((number_ + multiple_ - 1) / multiple_) * multiple_);
}

static
uint64_t
_align_uint64_t(uint64_t v_)
{
  return ((v_ + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1));
}

static
uint64_t
_dirent_size(const uint64_t namelen_)
{
  uint64_t rv;
  rv  = offsetof(fuse_dirent_t,name);
  rv += namelen_;
  rv  = _align_uint64_t(rv);
  return rv;
}

static
int
_dirents_buf_resize(fuse_dirents_t *d_,
                    const uint64_t  size_)
{
  if((kv_size(d_->data) + size_) >= kv_max(d_->data))
    {
      uint64_t new_size;
      new_size = _round_up((kv_size(d_->data) + size_),DENTS_BUF_EXPAND_SIZE);
      kv_resize(char,d_->data,new_size);
      if(d_->data.a == NULL)
        return -ENOMEM;
    }
  return 0;
}

static
fuse_dirent_t*
_dirents_dirent_alloc(fuse_dirents_t *d_,
                      const uint64_t  namelen_)
{
  int rv;
  uint64_t size;
  fuse_dirent_t *d;

  size = _dirent_size(namelen_);
  rv = _dirents_buf_resize(d_,size);
  if(rv)
    return NULL;

  d = (fuse_dirent_t*)&kv_end(d_->data);
  kv_size(d_->data) += size;

  return d;
}

int
fuse_dirents_add(fuse_dirents_t *d_,
                 const dirent   *de_,
                 const uint64_t  namelen_)
{
  fuse_dirent_t *d;

  d = _dirents_dirent_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,(uint32_t)kv_size(d_->data));
  d->ino     = de_->d_ino;
  d->namelen = (uint32_t)namelen_;
  d->type    = de_->d_type;
  memcpy(d->name,de_->d_name,namelen_);

  return 0;
}

int
fuse_dirents_add(fuse_dirents_t     *d_,
                 const fs::dirent64 *de_,
                 const uint64_t      namelen_)
{
  fuse_dirent_t *d;

  d = _dirents_dirent_alloc(d_,namelen_);
  if(d == NULL)
    return -ENOMEM;

  d->off     = kv_size(d_->offs);
  kv_push(uint32_t,d_->offs,(uint32_t)kv_size(d_->data));
  d->ino     = de_->ino;
  d->namelen = (uint32_t)namelen_;
  d->type    = de_->type;
  memcpy(d->name,de_->name,namelen_);

  return 0;
}

void
fuse_dirents_reset(fuse_dirents_t *d_)
{
  kv_size(d_->data) = 0;
  kv_size(d_->offs) = 1;
}

int
fuse_dirents_init(fuse_dirents_t *d_)
{
  kv_init(d_->data);
  kv_resize(char,d_->data,DENTS_BUF_EXPAND_SIZE);
  if(d_->data.a == NULL)
    return -ENOMEM;

  kv_init(d_->offs);
  kv_resize(uint32_t,d_->offs,DENTS_OFFS_INITIAL_CAPACITY);
  kv_push(uint32_t,d_->offs,0);

  return 0;
}

void
fuse_dirents_free(fuse_dirents_t *d_)
{
  kv_destroy(d_->data);
  kv_destroy(d_->offs);
}


// ---------------------------------------------------------------------------
// fuse_passthrough — Linux-specific, stub on Windows
// ---------------------------------------------------------------------------

extern "C"
int
fuse_passthrough_open(const int fd_)
{
  (void)fd_;
  return 0;  // INVALID_BACKING_ID
}

extern "C"
int
fuse_passthrough_close(const int backing_id_)
{
  (void)backing_id_;
  return 0;
}


// ---------------------------------------------------------------------------
// fuse_gc — garbage collection (Linux kernel FUSE internals, stub on Windows)
// ---------------------------------------------------------------------------

extern "C"
void
fuse_gc1()
{
  // No-op on Windows — WinFSP manages its own caches
}

extern "C"
void
fuse_gc()
{
  // No-op on Windows — WinFSP manages its own caches
}

extern "C"
void
fuse_invalidate_all_nodes()
{
  // No-op on Windows — WinFSP manages node invalidation
}


#endif // _WIN32
