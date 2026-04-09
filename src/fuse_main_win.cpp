/*
  Windows implementation of fuse_main().

  This file includes the vendored libfuse headers (mergerfs custom API).
  It extracts function pointers from the mergerfs fuse_operations struct
  and passes them to the WinFSP bridge via a type-safe intermediate
  struct that uses void* for fuse-specific types.
*/

#ifdef _WIN32

#include "fuse.h"              // vendored — mergerfs custom fuse_operations
#include "fuse_dirents.hpp"    // vendored — fuse_dirents_t
#include "winfsp_bridge.h"     // bridge API (no fuse headers)

#include <cstdio>
#include <cstring>


// The vendored fuse.h declares fuse_main as extern "C", so our
// definition automatically gets C linkage.

int
fuse_main(int                          argc,
          char                        *argv[],
          const struct fuse_operations *op)
{
  struct winfsp_bridge_ops bops;
  memset(&bops, 0, sizeof(bops));

  // Extract function pointers, casting to void*-based types.
  // The actual argument types (fuse_req_ctx_t*, struct stat*, etc.) are
  // ABI-compatible with the void* signatures in winfsp_bridge.h because
  // all pointer types have the same representation on x86_64.

  if(op->getattr)
    bops.getattr     = (mfs_getattr_fn)op->getattr;
  if(op->init)
    bops.init        = (mfs_init_fn)op->init;
  if(op->destroy)
    bops.destroy     = (mfs_destroy_fn)op->destroy;
  if(op->opendir)
    bops.opendir     = (mfs_opendir_fn)op->opendir;
  if(op->readdir)
    bops.readdir     = (mfs_readdir_fn)op->readdir;
  if(op->releasedir)
    bops.releasedir  = (mfs_releasedir_fn)op->releasedir;
  if(op->open)
    bops.open        = (mfs_open_fn)op->open;
  if(op->release)
    bops.release     = (mfs_release_fn)op->release;
  if(op->read)
    bops.read        = (mfs_read_fn)op->read;
  if(op->statfs)
    bops.statfs      = (mfs_statfs_fn)op->statfs;
  if(op->access)
    bops.access      = (mfs_access_fn)op->access;

  /* Tier 2 */
  if(op->write)
    bops.write       = (mfs_write_fn)op->write;
  if(op->create)
    bops.create      = (mfs_create_fn)op->create;
  if(op->mkdir)
    bops.mkdir       = (mfs_mkdir_fn)op->mkdir;
  if(op->unlink)
    bops.unlink      = (mfs_unlink_fn)op->unlink;
  if(op->rmdir)
    bops.rmdir       = (mfs_rmdir_fn)op->rmdir;
  if(op->rename)
    bops.rename      = (mfs_rename_fn)op->rename;
  if(op->truncate)
    bops.truncate    = (mfs_truncate_fn)op->truncate;
  if(op->ftruncate)
    bops.ftruncate   = (mfs_ftruncate_fn)op->ftruncate;
  if(op->chmod)
    bops.chmod       = (mfs_chmod_fn)op->chmod;
  if(op->chown)
    bops.chown       = (mfs_chown_fn)op->chown;
  if(op->utimens)
    bops.utimens     = (mfs_utimens_fn)op->utimens;
  if(op->flush)
    bops.flush       = (mfs_flush_fn)op->flush;
  if(op->fsync)
    bops.fsync       = (mfs_fsync_fn)op->fsync;
  if(op->readlink)
    bops.readlink    = (mfs_readlink_fn)op->readlink;
  if(op->symlink)
    bops.symlink     = (mfs_symlink_fn)op->symlink;
  if(op->link)
    bops.link        = (mfs_link_fn)op->link;

  /* Tier 3 */
  if(op->getxattr)
    bops.getxattr    = (mfs_getxattr_fn)op->getxattr;
  if(op->setxattr)
    bops.setxattr    = (mfs_setxattr_fn)op->setxattr;
  if(op->listxattr)
    bops.listxattr   = (mfs_listxattr_fn)op->listxattr;
  if(op->removexattr)
    bops.removexattr = (mfs_removexattr_fn)op->removexattr;
  if(op->ioctl)
    bops.ioctl       = (mfs_ioctl_fn)op->ioctl;

  winfsp_bridge_set_ops(&bops);

  return winfsp_bridge_run(argc, argv);
}


#endif // _WIN32
