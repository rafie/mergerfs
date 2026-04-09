/*
  Bridge between mergerfs's custom vendored libfuse API and WinFSP.

  This header is shared between fuse_main_win.cpp (which includes vendored
  headers) and winfsp_bridge.cpp (which includes WinFSP headers).  It
  deliberately includes NO fuse headers to avoid type conflicts.
*/

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  Function pointer types for mergerfs operations.

  These use void* for fuse-specific types to avoid depending on either
  the vendored or WinFSP header definitions.

  ctx      = const fuse_req_ctx_t*
  ffi      = fuse_file_info_t* or const fuse_file_info_t*
  stat_buf = struct stat*
  timeout  = fuse_timeouts_t*
  stv_buf  = struct statvfs*
  dirents  = fuse_dirents_t*
*/

typedef int   (*mfs_getattr_fn)(const void *ctx, const char *path, void *stat_buf, void *timeout);
typedef void *(*mfs_init_fn)(void *conn_info);
typedef void  (*mfs_destroy_fn)(void);
typedef int   (*mfs_opendir_fn)(const void *ctx, const char *path, void *ffi);
typedef int   (*mfs_readdir_fn)(const void *ctx, const void *ffi, void *dirents);
typedef int   (*mfs_releasedir_fn)(const void *ctx, const void *ffi);
typedef int   (*mfs_open_fn)(const void *ctx, const char *path, void *ffi);
typedef int   (*mfs_release_fn)(const void *ctx, const void *ffi);
typedef int   (*mfs_read_fn)(const void *ctx, const void *ffi, char *buf, size_t size, int64_t off);
typedef int   (*mfs_statfs_fn)(const void *ctx, const char *path, void *stv_buf);
typedef int   (*mfs_access_fn)(const void *ctx, const char *path, int mask);

/* Read-write ops (Tier 2) */
typedef int   (*mfs_write_fn)(const void *ctx, const void *ffi, const char *buf, size_t size, int64_t off);
typedef int   (*mfs_create_fn)(const void *ctx, const char *path, uint32_t mode, void *ffi);
typedef int   (*mfs_mkdir_fn)(const void *ctx, const char *path, uint32_t mode);
typedef int   (*mfs_unlink_fn)(const void *ctx, const char *path);
typedef int   (*mfs_rmdir_fn)(const void *ctx, const char *path);
typedef int   (*mfs_rename_fn)(const void *ctx, const char *oldpath, const char *newpath);
typedef int   (*mfs_truncate_fn)(const void *ctx, const char *path, int64_t size);
typedef int   (*mfs_ftruncate_fn)(const void *ctx, uint64_t fh, int64_t size);
typedef int   (*mfs_chmod_fn)(const void *ctx, const char *path, uint32_t mode);
typedef int   (*mfs_chown_fn)(const void *ctx, const char *path, uint32_t uid, uint32_t gid);
typedef int   (*mfs_utimens_fn)(const void *ctx, const char *path, const void *tv);
typedef int   (*mfs_flush_fn)(const void *ctx, const void *ffi);
typedef int   (*mfs_fsync_fn)(const void *ctx, uint64_t fh, int datasync);
typedef int   (*mfs_readlink_fn)(const void *ctx, const char *path, char *buf, size_t size);
typedef int   (*mfs_symlink_fn)(const void *ctx, const char *target, const char *linkpath, void *stat_buf, void *timeout);
typedef int   (*mfs_link_fn)(const void *ctx, const char *oldpath, const char *newpath, void *stat_buf, void *timeout);

/* Extended attrs (Tier 3) */
typedef int   (*mfs_getxattr_fn)(const void *ctx, const char *path, const char *name, char *value, size_t size);
typedef int   (*mfs_setxattr_fn)(const void *ctx, const char *path, const char *name, const char *value, size_t size, int flags);
typedef int   (*mfs_listxattr_fn)(const void *ctx, const char *path, char *list, size_t size);
typedef int   (*mfs_removexattr_fn)(const void *ctx, const char *path, const char *name);

/* ioctl */
typedef int   (*mfs_ioctl_fn)(const void *ctx, const void *ffi, unsigned long cmd, void *arg, unsigned int flags, void *data, uint32_t *out_bufsz);


struct winfsp_bridge_ops
{
  mfs_getattr_fn     getattr;
  mfs_init_fn        init;
  mfs_destroy_fn     destroy;
  mfs_opendir_fn     opendir;
  mfs_readdir_fn     readdir;
  mfs_releasedir_fn  releasedir;
  mfs_open_fn        open;
  mfs_release_fn     release;
  mfs_read_fn        read;
  mfs_statfs_fn      statfs;
  mfs_access_fn      access;

  /* Tier 2 */
  mfs_write_fn       write;
  mfs_create_fn      create;
  mfs_mkdir_fn       mkdir;
  mfs_unlink_fn      unlink;
  mfs_rmdir_fn       rmdir;
  mfs_rename_fn      rename;
  mfs_truncate_fn    truncate;
  mfs_ftruncate_fn   ftruncate;
  mfs_chmod_fn       chmod;
  mfs_chown_fn       chown;
  mfs_utimens_fn     utimens;
  mfs_flush_fn       flush;
  mfs_fsync_fn       fsync;
  mfs_readlink_fn    readlink;
  mfs_symlink_fn     symlink;
  mfs_link_fn        link;

  /* Tier 3 */
  mfs_getxattr_fn    getxattr;
  mfs_setxattr_fn    setxattr;
  mfs_listxattr_fn   listxattr;
  mfs_removexattr_fn removexattr;
  mfs_ioctl_fn       ioctl;
};

/*
  Set the mergerfs operation handlers.
  Called from fuse_main_win.cpp before starting the FUSE loop.
*/
void winfsp_bridge_set_ops(const struct winfsp_bridge_ops *ops);

/*
  Run the WinFSP FUSE main loop.
  Calls fsp_fuse_main_real with adapter callbacks.
*/
int winfsp_bridge_run(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
