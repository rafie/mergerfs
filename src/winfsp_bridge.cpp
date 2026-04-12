/*
  WinFSP bridge — adapts WinFSP FUSE 2.x callbacks to mergerfs operations.

  This file includes ONLY WinFSP headers (not vendored libfuse headers)
  to avoid type conflicts.  It communicates with the mergerfs side through
  the void*-based function pointers in winfsp_bridge.h.
*/

#ifdef _WIN32

// Prevent the compat shim headers from being picked up.
// We want the real Windows headers + WinFSP's own types.
#define _COMPAT_UNISTD_H
#define _COMPAT_SYS_STAT_H
#define _COMPAT_SYS_STATVFS_H
#define _COMPAT_SYS_TIME_H
#define _COMPAT_DIRENT_H
#define _COMPAT_SYSLOG_H
#define _COMPAT_PTHREAD_H
#define _COMPAT_GRP_H
#define _COMPAT_SYS_FILE_H
#define _COMPAT_SYS_IOCTL_H
#define _COMPAT_SYS_MOUNT_H
#define _COMPAT_SYS_RESOURCE_H
#define _COMPAT_SYS_SYSCALL_H
#define _COMPAT_SYS_SYSMACROS_H
#define _COMPAT_SYS_UIO_H
#define _COMPAT_GLOB_H
#define _COMPAT_FNMATCH_H

// Include WinFSP FUSE headers
#include <fuse/fuse.h>
#include <fuse/fuse_opt.h>

// Undefine WinFSP's fuse_main macro — we implement our own fuse_main
#undef fuse_main

#include "winfsp_bridge.h"
#include "msys_path.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Mergerfs request context — matches vendored fuse_req_ctx.h
// ---------------------------------------------------------------------------
struct mergerfs_req_ctx
{
  uint32_t len;
  uint32_t opcode;
  uint64_t unique;
  uint64_t nodeid;
  uint32_t uid;
  uint32_t gid;
  uint32_t pid;
  uint32_t umask;
};

// ---------------------------------------------------------------------------
// Mergerfs fuse_file_info_t — matches vendored fuse_common.h
// ---------------------------------------------------------------------------
struct mergerfs_file_info
{
  int      flags;
  uint32_t writepage    : 1;
  uint32_t direct_io    : 1;
  uint32_t keep_cache   : 1;
  uint32_t flush        : 1;
  uint32_t nonseekable  : 1;
  uint32_t flock_release: 1;
  uint32_t cache_readdir: 1;
  uint32_t auto_cache   : 1;
  uint32_t parallel_direct_writes : 1;
  uint32_t noflush      : 1;
  uint32_t passthrough  : 1;
  uint64_t fh;
  int32_t  backing_id;
  uint64_t lock_owner;
};

// ---------------------------------------------------------------------------
// POSIX struct stat compatible with our compat/sys/stat.h
// This must match the layout produced by compat/sys/stat.h on MSVC.
// The compat stat wraps _stat64 and adds st_atim/st_mtim/st_ctim.
// ---------------------------------------------------------------------------
struct compat_timespec
{
  int64_t tv_sec;
  long    tv_nsec;
};

// Must match struct stat in compat/sys/stat.h exactly
struct compat_stat
{
  uint32_t st_dev;       // dev_t = uint32_t
  uint64_t st_ino;       // uint64_t
  uint32_t st_mode;      // mode_t = uint32_t
  uint16_t st_nlink;     // nlink_t = uint16_t
  uint32_t st_uid;       // uid_t = uint32_t
  uint32_t st_gid;       // gid_t = uint32_t
  uint32_t st_rdev;      // dev_t = uint32_t
  int64_t  st_size;      // int64_t
  struct compat_timespec st_atim;
  struct compat_timespec st_mtim;
  struct compat_timespec st_ctim;
  int32_t  st_blksize;   // blksize_t = int32_t
  int64_t  st_blocks;    // blkcnt_t = int64_t
};

// ---------------------------------------------------------------------------
// POSIX struct statvfs compatible with our compat/sys/statvfs.h
// ---------------------------------------------------------------------------
struct compat_statvfs
{
  uint64_t f_bsize;
  uint64_t f_frsize;
  uint64_t f_blocks;
  uint64_t f_bfree;
  uint64_t f_bavail;
  uint64_t f_files;
  uint64_t f_ffree;
  uint64_t f_favail;
  uint64_t f_fsid;
  uint64_t f_flag;
  uint64_t f_namemax;
};

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static struct winfsp_bridge_ops g_ops;

extern "C"
void
winfsp_bridge_set_ops(const struct winfsp_bridge_ops *ops)
{
  memcpy(&g_ops, ops, sizeof(g_ops));
}

// ---------------------------------------------------------------------------
// Helper: create a dummy mergerfs request context from WinFSP context
// ---------------------------------------------------------------------------
static
void
_make_ctx(struct mergerfs_req_ctx *ctx)
{
  memset(ctx, 0, sizeof(*ctx));
  struct fuse_context *fc = fuse_get_context();
  if(fc)
    {
      ctx->uid   = fc->uid;
      ctx->gid   = fc->gid;
      ctx->pid   = fc->pid;
      ctx->umask = fc->umask;
    }
}

// ---------------------------------------------------------------------------
// Helper: convert WinFSP fuse_file_info <-> mergerfs fuse_file_info_t
// ---------------------------------------------------------------------------
static
void
_winfsp_to_mergerfs_fi(const struct fuse_file_info *wfi,
                       struct mergerfs_file_info   *mfi)
{
  memset(mfi, 0, sizeof(*mfi));
  if(!wfi) return;
  mfi->flags        = wfi->flags;
  mfi->writepage    = wfi->writepage;
  mfi->direct_io    = wfi->direct_io;
  mfi->keep_cache   = wfi->keep_cache;
  mfi->flush        = wfi->flush;
  mfi->nonseekable  = wfi->nonseekable;
  mfi->fh           = wfi->fh;
  mfi->lock_owner   = wfi->lock_owner;
}

static
void
_mergerfs_to_winfsp_fi(const struct mergerfs_file_info *mfi,
                       struct fuse_file_info           *wfi)
{
  if(!mfi || !wfi) return;
  wfi->flags       = mfi->flags;
  wfi->writepage   = mfi->writepage;
  wfi->direct_io   = mfi->direct_io;
  wfi->keep_cache  = mfi->keep_cache;
  wfi->flush       = mfi->flush;
  wfi->nonseekable = mfi->nonseekable;
  wfi->fh          = mfi->fh;
  wfi->lock_owner  = mfi->lock_owner;
}

// ---------------------------------------------------------------------------
// Helper: convert compat_stat -> WinFSP fuse_stat
// ---------------------------------------------------------------------------
static
void
_compat_to_fuse_stat(const struct compat_stat *cs,
                     struct fuse_stat         *fs)
{
  memset(fs, 0, sizeof(*fs));
  fs->st_dev     = cs->st_dev;
  fs->st_ino     = cs->st_ino;
  fs->st_mode    = cs->st_mode;
  fs->st_nlink   = cs->st_nlink;
  fs->st_uid     = cs->st_uid;
  fs->st_gid     = cs->st_gid;
  fs->st_rdev    = cs->st_rdev;
  fs->st_size    = cs->st_size;
  fs->st_atim.tv_sec  = cs->st_atim.tv_sec;
  fs->st_atim.tv_nsec = cs->st_atim.tv_nsec;
  fs->st_mtim.tv_sec  = cs->st_mtim.tv_sec;
  fs->st_mtim.tv_nsec = cs->st_mtim.tv_nsec;
  fs->st_ctim.tv_sec  = cs->st_ctim.tv_sec;
  fs->st_ctim.tv_nsec = cs->st_ctim.tv_nsec;
  fs->st_blksize = (fuse_blksize_t)cs->st_blksize;
  fs->st_blocks  = cs->st_blocks;
}

// ---------------------------------------------------------------------------
// Helper: convert compat_statvfs -> WinFSP fuse_statvfs
// ---------------------------------------------------------------------------
static
void
_compat_to_fuse_statvfs(const struct compat_statvfs *cs,
                        struct fuse_statvfs         *fs)
{
  memset(fs, 0, sizeof(*fs));
  fs->f_bsize   = cs->f_bsize;
  fs->f_frsize  = cs->f_frsize;
  fs->f_blocks  = cs->f_blocks;
  fs->f_bfree   = cs->f_bfree;
  fs->f_bavail  = cs->f_bavail;
  fs->f_files   = cs->f_files;
  fs->f_ffree   = cs->f_ffree;
  fs->f_favail  = cs->f_favail;
  fs->f_fsid    = cs->f_fsid;
  fs->f_flag    = cs->f_flag;
  fs->f_namemax = cs->f_namemax;
}


// ===========================================================================
// WinFSP adapter callbacks
// ===========================================================================

static int
winfsp_getattr(const char *path, struct fuse_stat *stbuf)
{
  if(!g_ops.getattr) return -ENOSYS;

  struct mergerfs_req_ctx ctx;
  struct compat_stat cs;
  uint64_t timeout[2] = {};  // fuse_timeouts_t: {entry, attr}

  _make_ctx(&ctx);
  memset(&cs, 0, sizeof(cs));

  int rv = g_ops.getattr(&ctx, path, &cs, timeout);
  if(rv == 0)
    {
      // On Windows, override uid/gid to match the requesting user.
      // WinFSP constructs Windows security descriptors from POSIX
      // mode + uid/gid. If uid doesn't match the calling user's
      // mapped UID, the owner permission bits won't apply, which
      // causes unexpected "Permission denied" on writable files.
      cs.st_uid = ctx.uid;
      cs.st_gid = ctx.gid;
      _compat_to_fuse_stat(&cs, stbuf);
    }

  return rv;
}

// Matches vendored fuse_conn_info_t layout (4 fields, no vtable)
struct mergerfs_conn_info
{
  uint32_t proto_major;
  uint32_t proto_minor;
  uint64_t capable;
  uint64_t want;
};

static void *
winfsp_init(struct fuse_conn_info *conn)
{
  if(!g_ops.init) return NULL;

  // Create a dummy mergerfs conn_info with no capabilities.
  // mergerfs init checks capable bits before setting want bits,
  // so all Linux-specific features will be skipped.
  struct mergerfs_conn_info mci;
  memset(&mci, 0, sizeof(mci));
  return g_ops.init(&mci);
}

static void
winfsp_destroy(void *data)
{
  (void)data;
  if(g_ops.destroy)
    g_ops.destroy();
}

static int
winfsp_opendir(const char *path, struct fuse_file_info *fi)
{
  if(!g_ops.opendir) return -ENOSYS;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;

  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  int rv = g_ops.opendir(&ctx, path, &mfi);
  if(rv == 0)
    _mergerfs_to_winfsp_fi(&mfi, fi);

  return rv;
}

static int
winfsp_readdir(const char             *path,
               void                   *buf,
               fuse_fill_dir_t         filler,
               fuse_off_t              offset,
               struct fuse_file_info  *fi)
{
  if(!g_ops.readdir) return -ENOSYS;

  (void)path;
  (void)offset;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;

  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  // Allocate a dirents buffer for mergerfs to fill
  // We import fuse_dirents_init/free/reset from fuse_lib_win.cpp
  // but we can't include the vendored header here.
  // Instead, we'll use a simple inline buffer approach.

  // fuse_dirents_t layout from vendored: { kvec_t(char) data; kvec_t(uint32_t) offs; }
  // kvec_t(T) = struct { size_t n, m; T *a; }
  struct kvec_char   { size_t n, m; char     *a; };
  struct kvec_u32    { size_t n, m; uint32_t *a; };
  struct dirents_buf { struct kvec_char data; struct kvec_u32 offs; };

  // Initialize
  struct dirents_buf d;
  memset(&d, 0, sizeof(d));
  d.data.m = 32 * 1024;
  d.data.a = (char*)malloc(d.data.m);
  if(!d.data.a) return -ENOMEM;
  d.data.n = 0;
  d.offs.m = 64;
  d.offs.a = (uint32_t*)malloc(d.offs.m * sizeof(uint32_t));
  if(!d.offs.a) { free(d.data.a); return -ENOMEM; }
  d.offs.a[0] = 0;
  d.offs.n = 1;

  int rv = g_ops.readdir(&ctx, &mfi, &d);


  if(rv == 0)
    {
      // Iterate the fuse_dirent_t entries in the data buffer.
      // fuse_dirent_t: { u64 ino; u64 off; u32 namelen; u32 type; char name[]; }
      size_t pos = 0;
      while(pos < d.data.n)
        {
          struct {
            uint64_t ino;
            uint64_t off;
            uint32_t namelen;
            uint32_t type;
          } hdr;

          if(pos + sizeof(hdr) > d.data.n)
            break;

          memcpy(&hdr, d.data.a + pos, sizeof(hdr));

          if(pos + sizeof(hdr) + hdr.namelen > d.data.n)
            break;

          // Null-terminate the name for the filler callback
          char namebuf[1024];
          size_t copylen = hdr.namelen < sizeof(namebuf)-1 ? hdr.namelen : sizeof(namebuf)-1;
          memcpy(namebuf, d.data.a + pos + sizeof(hdr), copylen);
          namebuf[copylen] = '\0';

          // Create a minimal fuse_stat for the filler
          struct fuse_stat st;
          memset(&st, 0, sizeof(st));
          st.st_ino  = hdr.ino;
          st.st_mode = (hdr.type << 12);  // DT_* to S_IF* conversion

          filler(buf, namebuf, &st, 0);

          // Advance to next entry (aligned to uint64_t)
          size_t entry_size = sizeof(hdr) + hdr.namelen;
          entry_size = (entry_size + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1);
          pos += entry_size;
        }
    }

  free(d.offs.a);
  free(d.data.a);

  return rv;
}

static int
winfsp_releasedir(const char *path, struct fuse_file_info *fi)
{
  if(!g_ops.releasedir) return 0;

  (void)path;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;

  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  return g_ops.releasedir(&ctx, &mfi);
}

static int
winfsp_open(const char *path, struct fuse_file_info *fi)
{
  if(!g_ops.open) return -ENOSYS;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;

  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  int rv = g_ops.open(&ctx, path, &mfi);
  if(rv == 0)
    _mergerfs_to_winfsp_fi(&mfi, fi);

  return rv;
}

static int
winfsp_release(const char *path, struct fuse_file_info *fi)
{
  if(!g_ops.release) return 0;

  (void)path;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;

  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  return g_ops.release(&ctx, &mfi);
}

static int
winfsp_read(const char            *path,
            char                  *buf,
            size_t                 size,
            fuse_off_t             off,
            struct fuse_file_info *fi)
{
  if(!g_ops.read) return -ENOSYS;

  (void)path;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;

  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  return g_ops.read(&ctx, &mfi, buf, size, off);
}

static int
winfsp_statfs(const char *path, struct fuse_statvfs *stbuf)
{
  if(!g_ops.statfs) return -ENOSYS;

  struct mergerfs_req_ctx ctx;
  struct compat_statvfs csv;

  _make_ctx(&ctx);
  memset(&csv, 0, sizeof(csv));

  int rv = g_ops.statfs(&ctx, path, &csv);
  if(rv == 0)
    _compat_to_fuse_statvfs(&csv, stbuf);

  return rv;
}

static int
winfsp_access(const char *path, int mask)
{
  if(!g_ops.access) return -ENOSYS;

  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);

  return g_ops.access(&ctx, path, mask);
}

// ---------------------------------------------------------------------------
// Tier 2 adapters (read-write support)
// ---------------------------------------------------------------------------

static int
winfsp_write(const char            *path,
             const char            *buf,
             size_t                 size,
             fuse_off_t             off,
             struct fuse_file_info *fi)
{
  if(!g_ops.write) return -ENOSYS;
  (void)path;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;
  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  return g_ops.write(&ctx, &mfi, buf, size, off);
}

static int
winfsp_create(const char *path, fuse_mode_t mode, struct fuse_file_info *fi)
{
  if(!g_ops.create) return -ENOSYS;

  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;
  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);

  int rv = g_ops.create(&ctx, path, mode, &mfi);
  if(rv == 0)
    _mergerfs_to_winfsp_fi(&mfi, fi);

  return rv;
}

static int
winfsp_mkdir(const char *path, fuse_mode_t mode)
{
  if(!g_ops.mkdir) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.mkdir(&ctx, path, mode);
}

static int
winfsp_unlink(const char *path)
{
  if(!g_ops.unlink) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.unlink(&ctx, path);
}

static int
winfsp_rmdir(const char *path)
{
  if(!g_ops.rmdir) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.rmdir(&ctx, path);
}

static int
winfsp_rename(const char *oldpath, const char *newpath)
{
  if(!g_ops.rename) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.rename(&ctx, oldpath, newpath);
}

static int
winfsp_truncate(const char *path, fuse_off_t size)
{
  if(!g_ops.truncate) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.truncate(&ctx, path, size);
}

static int
winfsp_ftruncate(const char *path, fuse_off_t off, struct fuse_file_info *fi)
{
  if(!g_ops.ftruncate) return -ENOSYS;
  (void)path;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.ftruncate(&ctx, fi ? fi->fh : 0, off);
}

static int
winfsp_chmod(const char *path, fuse_mode_t mode)
{
  if(!g_ops.chmod) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.chmod(&ctx, path, mode);
}

static int
winfsp_chown(const char *path, fuse_uid_t uid, fuse_gid_t gid)
{
  if(!g_ops.chown) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.chown(&ctx, path, uid, gid);
}

static int
winfsp_utimens(const char *path, const struct fuse_timespec tv[2])
{
  if(!g_ops.utimens) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  // fuse_timespec and our compat timespec may differ in layout.
  // Convert fuse_timespec (int64_t sec, int64_t nsec) to compat timespec.
  struct { int64_t tv_sec; long tv_nsec; } cts[2];
  if(tv)
    {
      cts[0].tv_sec  = tv[0].tv_sec;
      cts[0].tv_nsec = (long)tv[0].tv_nsec;
      cts[1].tv_sec  = tv[1].tv_sec;
      cts[1].tv_nsec = (long)tv[1].tv_nsec;
    }
  return g_ops.utimens(&ctx, path, tv ? cts : NULL);
}

static int
winfsp_flush(const char *path, struct fuse_file_info *fi)
{
  if(!g_ops.flush) return 0;
  (void)path;
  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;
  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);
  return g_ops.flush(&ctx, &mfi);
}

static int
winfsp_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
  if(!g_ops.fsync) return 0;
  (void)path;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.fsync(&ctx, fi ? fi->fh : 0, datasync);
}

static int
winfsp_readlink(const char *path, char *buf, size_t size)
{
  if(!g_ops.readlink) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.readlink(&ctx, path, buf, size);
}

static int
winfsp_symlink(const char *target, const char *linkpath)
{
  if(!g_ops.symlink) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  struct compat_stat cs;
  uint64_t timeout[2] = {};
  _make_ctx(&ctx);
  memset(&cs, 0, sizeof(cs));
  return g_ops.symlink(&ctx, target, linkpath, &cs, timeout);
}

static int
winfsp_link(const char *oldpath, const char *newpath)
{
  if(!g_ops.link) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  struct compat_stat cs;
  uint64_t timeout[2] = {};
  _make_ctx(&ctx);
  memset(&cs, 0, sizeof(cs));
  return g_ops.link(&ctx, oldpath, newpath, &cs, timeout);
}

// ---------------------------------------------------------------------------
// Tier 3 adapters
// ---------------------------------------------------------------------------

static int
winfsp_getxattr(const char *path, const char *name, char *value, size_t size)
{
  if(!g_ops.getxattr) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.getxattr(&ctx, path, name, value, size);
}

static int
winfsp_setxattr(const char *path, const char *name,
                const char *value, size_t size, int flags)
{
  if(!g_ops.setxattr) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.setxattr(&ctx, path, name, value, size, flags);
}

static int
winfsp_listxattr(const char *path, char *list, size_t size)
{
  if(!g_ops.listxattr) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.listxattr(&ctx, path, list, size);
}

static int
winfsp_removexattr(const char *path, const char *name)
{
  if(!g_ops.removexattr) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  _make_ctx(&ctx);
  return g_ops.removexattr(&ctx, path, name);
}

static int
winfsp_ioctl(const char *path, int cmd, void *arg,
             struct fuse_file_info *fi, unsigned int flags, void *data)
{
  if(!g_ops.ioctl) return -ENOSYS;
  struct mergerfs_req_ctx ctx;
  struct mergerfs_file_info mfi;
  _make_ctx(&ctx);
  _winfsp_to_mergerfs_fi(fi, &mfi);
  uint32_t out_bufsz = 0;
  return g_ops.ioctl(&ctx, &mfi, (unsigned long)cmd, arg, flags, data, &out_bufsz);
}


// ===========================================================================
// winfsp_bridge_run — entry point
// ===========================================================================

extern "C"
int
winfsp_bridge_run(int argc, char *argv[])
{
  struct fuse_operations winfsp_ops;
  memset(&winfsp_ops, 0, sizeof(winfsp_ops));

  // Tier 1 — minimum viable mount
  winfsp_ops.getattr    = winfsp_getattr;
  winfsp_ops.init       = winfsp_init;
  winfsp_ops.destroy    = winfsp_destroy;
  winfsp_ops.opendir    = winfsp_opendir;
  winfsp_ops.readdir    = winfsp_readdir;
  winfsp_ops.releasedir = winfsp_releasedir;
  winfsp_ops.open       = winfsp_open;
  winfsp_ops.release    = winfsp_release;
  winfsp_ops.read       = winfsp_read;
  winfsp_ops.statfs     = winfsp_statfs;
  winfsp_ops.access     = winfsp_access;

  // Tier 2 — read-write support
  winfsp_ops.write      = winfsp_write;
  winfsp_ops.create     = winfsp_create;
  winfsp_ops.mkdir      = winfsp_mkdir;
  winfsp_ops.unlink     = winfsp_unlink;
  winfsp_ops.rmdir      = winfsp_rmdir;
  winfsp_ops.rename     = winfsp_rename;
  winfsp_ops.truncate   = winfsp_truncate;
  winfsp_ops.ftruncate  = winfsp_ftruncate;
  winfsp_ops.chmod      = winfsp_chmod;
  winfsp_ops.chown      = winfsp_chown;
  winfsp_ops.utimens    = winfsp_utimens;
  winfsp_ops.flush      = winfsp_flush;
  winfsp_ops.fsync      = winfsp_fsync;
  winfsp_ops.readlink   = winfsp_readlink;
  winfsp_ops.symlink    = winfsp_symlink;
  winfsp_ops.link       = winfsp_link;

  // Tier 3
  winfsp_ops.setxattr    = winfsp_setxattr;
  winfsp_ops.getxattr    = winfsp_getxattr;
  winfsp_ops.listxattr   = winfsp_listxattr;
  winfsp_ops.removexattr = winfsp_removexattr;
  winfsp_ops.ioctl       = winfsp_ioctl;

  // Convert MSYS-style mountpoint arg (e.g. /m → M:\) for WinFSP
  std::vector<std::string> native_args;
  std::vector<char*> native_argv;
  for(int i = 0; i < argc; i++)
    native_args.push_back(msys_path::to_native(argv[i]));
  for(auto &a : native_args)
    native_argv.push_back(a.data());
  native_argv.push_back(nullptr);

  return fuse_main_real(argc, native_argv.data(), &winfsp_ops, sizeof(winfsp_ops), NULL);
}


#endif // _WIN32
