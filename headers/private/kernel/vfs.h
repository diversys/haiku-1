/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel.h>
#include <stage2.h>
#include <sys/stat.h>
#include <dirent.h>

#define DEFAULT_FD_TABLE_SIZE	128
#define MAX_FD_TABLE_SIZE		2048

#include <vfs_types.h>

/* macro to allocate a iovec array on the stack */
#define IOVECS(name, size) \
	uint8 _##name[sizeof(iovecs) + (size)*sizeof(iovec)]; \
	iovecs *name = (iovecs *)_##name

struct fs_info;

#ifdef __cplusplus
extern "C" {
#endif 


#if 0
struct fs_calls {
	int (*fs_mount)(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **priv_vnode_root);
	int (*fs_unmount)(void *fs_cookie);
	int (*fs_register_mountpoint)(void *fs_cookie, void *vnode, void *redir_vnode);
	int (*fs_unregister_mountpoint)(void *fs_cookie, void *vnode);
	int (*fs_dispose_vnode)(void *fs_cookie, void *vnode);
	int (*fs_open)(void *fs_cookie, void *base_vnode, const char *path, const char *stream, stream_type stream_type, void **vnode, void **cookie, struct redir_struct *redir);
	int (*fs_seek)(void *fs_cookie, void *vnode, void *cookie, off_t pos, int seek_type);
	int (*fs_read)(void *fs_cookie, void *vnode, void *cookie, void *buf, off_t pos, size_t *len);
	int (*fs_write)(void *fs_cookie, void *vnode, void *cookie, const void *buf, off_t pos, size_t *len);
	int (*fs_ioctl)(void *fs_cookie, void *vnode, void *cookie, ulong op, void *buf, size_t len);
	int (*fs_close)(void *fs_cookie, void *vnode, void *cookie);
	int (*fs_create)(void *fs_cookie, void *base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir);
	int (*fs_stat)(void *fs_cookie, void *base_vnode, const char *path, const char *stream, stream_type stream_type, struct vnode_stat *stat, struct redir_struct *redir);
};
#endif

struct fs_calls {
	/* general operations */
	int (*fs_mount)(fs_id id, const char *device, void *args, fs_cookie *_fs, vnode_id *_rootVnodeID);
	int (*fs_unmount)(fs_cookie fs);

	int (*fs_read_fs_info)(fs_cookie fs, struct fs_info *info);
	int (*fs_write_fs_info)(fs_cookie fs, struct fs_info *info, int mask);
	int (*fs_sync)(fs_cookie fs);

	/* vnode operations */
	int (*fs_lookup)(fs_cookie fs, fs_vnode dir, const char *name, vnode_id *_id);
	int (*fs_get_vnode_name)(fs_cookie fs, fs_vnode vnode, char *buffer, size_t bufferSize);

	int (*fs_get_vnode)(fs_cookie fs, vnode_id id, fs_vnode *_vnode, bool reenter);
	int (*fs_put_vnode)(fs_cookie fs, fs_vnode vnode, bool reenter);
	int (*fs_remove_vnode)(fs_cookie fs, fs_vnode vnode, bool reenter);

	/* VM file access */
	int (*fs_can_page)(fs_cookie fs, fs_vnode v);
	ssize_t (*fs_read_page)(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);
	ssize_t (*fs_write_page)(fs_cookie fs, fs_vnode v, iovecs *vecs, off_t pos);

	/* common operations */
	int (*fs_ioctl)(fs_cookie fs, fs_vnode v, file_cookie cookie, ulong op, void *buffer, size_t length);
	int (*fs_fsync)(fs_cookie fs, fs_vnode v);

	int (*fs_unlink)(fs_cookie fs, fs_vnode dir, const char *name);
	int (*fs_rename)(fs_cookie fs, fs_vnode olddir, const char *oldname, fs_vnode newdir, const char *newname);

	int (*fs_read_stat)(fs_cookie fs, fs_vnode v, struct stat *stat);
	int (*fs_write_stat)(fs_cookie fs, fs_vnode v, struct stat *stat, int statMask);

	/* file operations */
	int (*fs_create)(fs_cookie fs, fs_vnode dir, const char *name, int omode, int perms, file_cookie *_cookie, vnode_id *_newVnodeID);
	int (*fs_open)(fs_cookie fs, fs_vnode v, int oflags, file_cookie *_cookie);
	int (*fs_close)(fs_cookie fs, fs_vnode v, file_cookie cookie);
	int (*fs_free_cookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
	ssize_t (*fs_read)(fs_cookie fs, fs_vnode v, file_cookie cookie, void *buffer, off_t pos, size_t *length);
	ssize_t (*fs_write)(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buffer, off_t pos, size_t *length);
	off_t (*fs_seek)(fs_cookie fs, fs_vnode v, file_cookie cookie, off_t pos, int seekType);

	/* directory operations */
	int (*fs_create_dir)(fs_cookie fs, fs_vnode parent, const char *name, int perms, vnode_id *_newVnodeID);
	int (*fs_open_dir)(fs_cookie fs, fs_vnode v, file_cookie *_cookie);
	int (*fs_close_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie);
	int (*fs_free_dir_cookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
	int (*fs_read_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie, struct dirent *buffer, size_t bufferSize, uint32 *_num);
	int (*fs_rewind_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie);

	/* attribute directory operations */
//	int (*fs_open_attr_dir)(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags);
//	int (*fs_close_attr_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_free_attr_dir_cookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_read_attr_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie, struct dirent *buffer, size_t bufferSize, uint32 *_num);
//	int (*fs_rewind_attr_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//
//	/* attribute operations */
//	int (*fs_open_attr)(fs_cookie fs, fs_vnode v, file_cookie *cookie, stream_type st, int oflags);
//	int (*fs_close_attr)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_free_attr_cookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	ssize_t (*fs_read_attr)(fs_cookie fs, fs_vnode v, file_cookie cookie, void *buf, off_t pos, size_t *len);
//	ssize_t (*fs_write_attr)(fs_cookie fs, fs_vnode v, file_cookie cookie, const void *buf, off_t pos, size_t *len);
//	int (*fs_seek_attr)(fs_cookie fs, fs_vnode v, file_cookie cookie, off_t pos, int st);
//	int (*fs_rename_attr)(fs_cookie fs, fs_vnode file, const char *oldname, const char *newname);
//
//	/* index directory & index operations */
//	int (*fs_open_index_dir)(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags);
//	int (*fs_close_index_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_free_index_dir_cookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_read_index_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie, struct dirent *buffer, size_t bufferSize, uint32 *_num);
//	int (*fs_rewind_index_dir)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//
//	/* query operations */
//	int (*fs_open_query)(fs_cookie fs, fs_vnode v, file_cookie *cookie, int oflags);
//	int (*fs_close_query)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_free_query_cookie)(fs_cookie fs, fs_vnode v, file_cookie cookie);
//	int (*fs_read_query)(fs_cookie fs, fs_vnode v, file_cookie cookie, struct dirent *buffer, size_t bufferSize, uint32 *_num);
//	int (*fs_rewind_query)(fs_cookie fs, fs_vnode v, file_cookie cookie);
};

int vfs_init(kernel_args *ka);
int vfs_bootstrap_all_filesystems(void);
int vfs_register_filesystem(const char *name, struct fs_calls *calls);
void *vfs_new_io_context(void *parent_ioctx);
int vfs_free_io_context(void *ioctx);
int vfs_test(void);

struct rlimit;
int vfs_getrlimit(int resource, struct rlimit * rlp);
int vfs_setrlimit(int resource, const struct rlimit * rlp);

image_id vfs_load_fs_module(const char *path);

/* calls needed by fs internals */
int vfs_get_vnode(fs_id fsid, vnode_id vnid, fs_vnode *v);
int vfs_put_vnode(fs_id fsid, vnode_id vnid);
int vfs_remove_vnode(fs_id fsid, vnode_id vnid);

/* calls needed by the VM for paging */
int vfs_get_vnode_from_fd(int fd, bool kernel, void **vnode);
int vfs_get_vnode_from_path(const char *path, bool kernel, void **vnode);
int vfs_put_vnode_ptr(void *vnode);
void vfs_vnode_acquire_ref(void *vnode);
void vfs_vnode_release_ref(void *vnode);
ssize_t vfs_can_page(void *vnode);
ssize_t vfs_read_page(void *vnode, iovecs *vecs, off_t pos);
ssize_t vfs_write_page(void *vnode, iovecs *vecs, off_t pos);
void *vfs_get_cache_ptr(void *vnode);
int vfs_set_cache_ptr(void *vnode, void *cache);

/* calls kernel code should make for file I/O */
int sys_mount(const char *path, const char *device, const char *fs_name, void *args);
int sys_unmount(const char *path);
int sys_sync(void);
int sys_open_entry_ref(dev_t device, ino_t inode, const char *name, int omode);
int sys_open(const char *path, int omode);
int sys_open_dir_node_ref(dev_t device, ino_t inode);
int sys_open_dir_entry_ref(dev_t device, ino_t inode, const char *name);
int sys_open_dir(const char *path);
int sys_fsync(int fd);
off_t sys_seek(int fd, off_t pos, int seekType);
int sys_create_entry_ref(dev_t device, ino_t inode, const char *uname, int omode, int perms);
int sys_create(const char *path, int omode, int perms);
int sys_create_dir_entry_ref(dev_t device, ino_t inode, const char *name, int perms);
int sys_create_dir(const char *path, int perms);
int sys_unlink(const char *path);
int sys_rename(const char *oldpath, const char *newpath);
int sys_write_stat(const char *path, struct stat *stat, int stat_mask);
char *sys_getcwd(char *buf, size_t size);
int sys_setcwd(const char* path);

/* calls the syscall dispatcher should use for user file I/O */
int user_mount(const char *path, const char *device, const char *fs_name, void *args);
int user_unmount(const char *path);
int user_sync(void);
int user_open_entry_ref(dev_t device, ino_t inode, const char *name, int omode);
int user_open(const char *path, int omode);
int user_open_dir_node_ref(dev_t device, ino_t inode);
int user_open_dir_entry_ref(dev_t device, ino_t inode, const char *uname);
int user_open_dir(const char *path);
int user_fsync(int fd);
off_t user_seek(int fd, off_t pos, int seekType);
int user_create_entry_ref(dev_t device, ino_t inode, const char *uname, int omode, int perms);
int user_create(const char *path, int omode, int perms);
int user_create_dir_entry_ref(dev_t device, ino_t inode, const char *name, int perms);
int user_create_dir(const char *path, int perms);
int user_unlink(const char *path);
int user_rename(const char *oldpath, const char *newpath);
int user_read_stat(const char *path, struct stat *stat);
int user_write_stat(const char *path, struct stat *stat, int stat_mask);
int user_getcwd(char *buf, size_t size);
int user_setcwd(const char* path);

/* fd kernel prototypes (implementation located in fd.c) */
extern ssize_t sys_read(int fd, void *buf, off_t pos, size_t len);
extern ssize_t sys_write(int fd, const void *buf, off_t pos, size_t len);
extern int sys_ioctl(int fd, ulong cmd, void *data, size_t length);
extern ssize_t sys_read_dir(int fd, struct dirent *buffer, size_t bufferSize, uint32 maxCount);
extern status_t sys_rewind_dir(int fd);
extern int sys_read_stat(const char *path, struct stat *stat);
extern int sys_close(int fd);
extern int sys_dup(int fd);
extern int sys_dup2(int ofd, int nfd);

/* fd user prototypes (implementation located in fd.c)  */
extern ssize_t user_read(int fd, void *buf, off_t pos, size_t len);
extern ssize_t user_write(int fd, const void *buf, off_t pos, size_t len);
extern int user_ioctl(int fd, ulong cmd, void *data, size_t length);
extern ssize_t user_read_dir(int fd, struct dirent *buffer, size_t bufferSize, uint32 maxCount);
extern status_t user_rewind_dir(int fd);
extern int user_fstat(int, struct stat *);
extern int user_close(int fd);
extern int user_dup(int fd);
extern int user_dup2(int ofd, int nfd);

/* vfs entry points... */

#ifdef __cplusplus
}
#endif 

#endif	/* _KERNEL_VFS_H */
