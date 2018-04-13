/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Jffs2 FileSystem - system specific information.
 *
 * Copyright 2018 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _OS_PHOENIX_FS_H_
#define _OS_PHOENIX_FS_H_

#include <sys/stat.h>

#include "linux/list.h"
#include "types.h"

#define DT_UNKNOWN		0
#define DT_FIFO			1
#define DT_CHR			2
#define DT_DIR			4
#define DT_BLK			6
#define DT_REG			8
#define DT_LNK			10
#define DT_SOCK			12
#define DT_WHT			14

#define RENAME_NOREPLACE		(1 << 0)		/* Don't overwrite target */
#define RENAME_EXCHANGE			(1 << 1)		/* Exchange source and dest */
#define RENAME_WHITEOUT			(1 << 2)		/* Whiteout source */

#define S_IRWXUGO		(S_IRWXU|S_IRWXG|S_IRWXO)
#define S_IALLUGO		(S_ISUID|S_ISGID|S_ISVTX|S_IRWXUGO)
#define S_IRUGO			(S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO			(S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO			(S_IXUSR|S_IXGRP|S_IXOTH)

#define SB_RDONLY		 1		/* Mount read-only */
#define SB_NOSUID		 2		/* Ignore suid and sgid bits */
#define SB_NODEV		 4		/* Disallow access to device special files */
#define SB_NOEXEC		 8		/* Disallow program execution */
#define SB_SYNCHRONOUS	16		/* Writes are synced at once */
#define SB_MANDLOCK		64		/* Allow mandatory locks on an FS */
#define SB_DIRSYNC		128		/* Directory modifications are synchronous */
#define SB_NOATIME		1024	/* Do not update access times. */
#define SB_NODIRATIME	2048	/* Do not update directory access times */
#define SB_SILENT		32768
#define SB_POSIXACL		(1<<16)	/* VFS does not apply the umask */
#define SB_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define SB_I_VERSION	(1<<23) /* Update inode I_version field */
#define SB_LAZYTIME		(1<<25) /* Update the on-disk [acm]times lazily */


#define I_DIRTY_SYNC			(1 << 0)
#define I_DIRTY_DATASYNC		(1 << 1)

struct timespec current_time(struct inode *inode);


struct dir_context;
struct page;
struct address_space;
struct iattr;

typedef int (*filldir_t)(struct dir_context *, const char *, int, loff_t, u64,
					 unsigned);

struct dir_context {
	const filldir_t actor;
	loff_t pos;
};


struct file {
	struct inode *f_inode;
	int todo;
	struct address_space *f_mapping;
};


static inline struct inode *file_inode(const struct file *f)
{
	return f->f_inode;
}


static inline bool dir_emit_dots(struct file *file, struct dir_context *ctx)
{
	return 1;
}

static inline bool dir_emit(struct dir_context *ctx, const char *name,
		int namelen, u64 ino, unsigned type)
{
	return ctx->actor(ctx, name, namelen, ctx->pos, ino, type) == 0;
}


struct file_operations {
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iterate_shared) (struct file *, struct dir_context *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
};



struct address_space {
	struct inode							*host;		/* owner: inode, block_device */
//	struct radix_tree_root	page_tree;	/* radix tree of all pages */
	spinlock_t								tree_lock;	/* and lock protecting it */
	atomic_t								i_mmap_writable;/* count VM_SHARED mappings */
	struct rb_root							i_mmap;		/* tree of private and shared mappings */
//	struct rw_semaphore	i_mmap_rwsem;	/* protect tree, count, list */
	/* Protected by tree_lock together with the radix tree */
	unsigned long							nrpages;	/* number of total pages */
	/* number of shadow or DAX exceptional entries */
	unsigned long							nrexceptional;
//	pgoff_t			writeback_index;/* writeback starts here */
	const struct address_space_operations 	*a_ops;	/* methods */
	unsigned long							flags;		/* error bits/gfp mask */
	spinlock_t								private_lock;	/* for use by the address_space */
	struct list_head						private_list;	/* ditto */
	void									*private_data;	/* ditto */
} __attribute__((aligned(sizeof(long))));

struct inode_operations;

struct inode {
	ssize_t						i_size;
	umode_t						i_mode;
	unsigned long				i_ino;
	struct super_block			*i_sb;
	struct timespec				i_atime;
	struct timespec				i_mtime;
	struct timespec				i_ctime;
	struct inode_operations		*i_op;
	struct file_operations		*i_fop;
	struct address_space		*i_mapping;
	char						*i_link;
	unsigned int				i_nlink;
	blkcnt_t					i_blocks;
	dev_t						i_rdev;
	unsigned long				i_state;
	struct address_space		i_data;
	kuid_t						i_uid;
	kgid_t						i_gid;
};


struct super_block {
	void *s_fs_info;
	unsigned long s_magic;
	unsigned char s_blocksize_bits;
	unsigned long s_blocksize;
	unsigned long s_flags;
	loff_t s_maxbytes;
	struct dentry *s_root;
	int todo;
};

static inline bool sb_rdonly(const struct super_block *sb) { return sb->s_flags & SB_RDONLY; }

struct inode_operations {
	struct dentry * (*lookup) (struct inode *,struct dentry *, unsigned int);
	const char * (*get_link) (struct dentry *, struct inode *, struct delayed_call *);
	int (*permission) (struct inode *, int);
	struct posix_acl * (*get_acl)(struct inode *, int);

	int (*readlink) (struct dentry *, char __user *,int);

	int (*create) (struct inode *,struct dentry *, umode_t, bool);
	int (*link) (struct dentry *,struct inode *,struct dentry *);
	int (*unlink) (struct inode *,struct dentry *);
	int (*symlink) (struct inode *,struct dentry *,const char *);
	int (*mkdir) (struct inode *,struct dentry *,umode_t);
	int (*rmdir) (struct inode *,struct dentry *);
	int (*mknod) (struct inode *,struct dentry *,umode_t,dev_t);
	int (*rename) (struct inode *, struct dentry *,
			struct inode *, struct dentry *, unsigned int);
	int (*setattr) (struct dentry *, struct iattr *);
	int (*getattr) (const struct path *, struct kstat *, u32, unsigned int);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*update_time)(struct inode *, struct timespec *, int);
	int (*atomic_open)(struct inode *, struct dentry *,
			   struct file *, unsigned open_flag,
			   umode_t create_mode, int *opened);
	int (*tmpfile) (struct inode *, struct dentry *, umode_t);
	int (*set_acl)(struct inode *, struct posix_acl *, int);
};

struct address_space_operations {
//	int (*writepage)(struct page *page, struct writeback_control *wbc);
	int (*readpage)(struct file *, struct page *);

	/* Write back some dirty pages from this mapping. */
//	int (*writepages)(struct address_space *, struct writeback_control *);

	/* Set a page dirty.  Return true if this dirtied it */
//	int (*set_page_dirty)(struct page *page);

//	int (*readpages)(struct file *filp, struct address_space *mapping,
//			struct list_head *pages, unsigned nr_pages);

	int (*write_begin)(struct file *, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned flags,
				struct page **pagep, void **fsdata);
	int (*write_end)(struct file *, struct address_space *mapping,
				loff_t pos, unsigned len, unsigned copied,
				struct page *page, void *fsdata);

	/* Unfortunately this kludge is needed for FIBMAP. Don't use it */
//	sector_t (*bmap)(struct address_space *, sector_t);
//	void (*invalidatepage) (struct page *, unsigned int, unsigned int);
//	int (*releasepage) (struct page *, gfp_t);
//	void (*freepage)(struct page *);
//	ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *iter, loff_t offset);
	/*
	 * migrate the contents of a page to the specified target. If
	 * migrate_mode is MIGRATE_ASYNC, it must not block.
	 */
//	int (*migratepage) (struct address_space *,
//			struct page *, struct page *, enum migrate_mode);
//	int (*launder_page) (struct page *);
//	int (*is_partially_uptodate) (struct page *, unsigned long,
//					unsigned long);
//	void (*is_dirty_writeback) (struct page *, bool *, bool *);
//	int (*error_remove_page)(struct address_space *, struct page *);

	/* swapfile support */
//	int (*swap_activate)(struct swap_info_struct *sis, struct file *file,
//				sector_t *span);
//	void (*swap_deactivate)(struct file *file);
};

#define ATTR_MODE	(1 << 0)
#define ATTR_UID	(1 << 1)
#define ATTR_GID	(1 << 2)
#define ATTR_SIZE	(1 << 3)
#define ATTR_ATIME	(1 << 4)
#define ATTR_MTIME	(1 << 5)
#define ATTR_CTIME	(1 << 6)
#define ATTR_ATIME_SET	(1 << 7)
#define ATTR_MTIME_SET	(1 << 8)
#define ATTR_FORCE	(1 << 9) /* Not a change, but a change it */
#define ATTR_ATTR_FLAG	(1 << 10)
#define ATTR_KILL_SUID	(1 << 11)
#define ATTR_KILL_SGID	(1 << 12)
#define ATTR_FILE	(1 << 13)
#define ATTR_KILL_PRIV	(1 << 14)
#define ATTR_OPEN	(1 << 15) /* Truncating from open(O_TRUNC) */
#define ATTR_TIMES_SET	(1 << 16)
#define ATTR_TOUCH	(1 << 17)

struct iattr {
	unsigned int	ia_valid;
	umode_t		ia_mode;
	kuid_t		ia_uid;
	kgid_t		ia_gid;
	loff_t		ia_size;
	struct timespec	ia_atime;
	struct timespec	ia_mtime;
	struct timespec	ia_ctime;

	/*
	 * Not an attribute, but an auxiliary info for filesystems wanting to
	 * implement an ftruncate() like method.  NOTE: filesystem should
	 * check for (ia_valid & ATTR_FILE), and not for (ia_file != NULL).
	 */
	struct file	*ia_file;
};

int setattr_prepare(struct dentry *dentry, struct iattr *iattr);

static inline int posix_acl_chmod(struct inode *inode, umode_t mode)
{
	return 0;
}

void init_special_inode(struct inode *inode, umode_t mode, dev_t dev);

void inc_nlink(struct inode *inode);

void clear_nlink(struct inode *inode);

void set_nlink(struct inode *inode, unsigned int nlink);

void drop_nlink(struct inode *inode);

void ihold(struct inode * inode);

struct inode *new_inode(struct super_block *sb);

void unlock_new_inode(struct inode *inode);

void iget_failed(struct inode *inode);

struct inode * iget_locked(struct super_block *sb, unsigned long ino);

void iput(struct inode *inode);

static inline void inode_lock(struct inode *inode)
{
//	mutex_lock(&inode->i_mutex);
}

static inline void inode_unlock(struct inode *inode)
{
//	mutex_unlock(&inode->i_mutex);
}

void clear_inode(struct inode *inode);

bool is_bad_inode(struct inode *inode);

struct inode *ilookup(struct super_block *sb, unsigned long ino);

int insert_inode_locked(struct inode *inode);

void make_bad_inode(struct inode *inode);

/* Helper functions so that in most cases filesystems will
 * not need to deal directly with kuid_t and kgid_t and can
 * instead deal with the raw numeric values that are stored
 * in the filesystem.
 */
static inline uid_t i_uid_read(const struct inode *inode)
{
	return 0;//inode->i_uid;
}


static inline gid_t i_gid_read(const struct inode *inode)
{
	return 0;//inode->i_gid;
}

static inline void i_uid_write(struct inode *inode, uid_t uid)
{
	//inode->i_uid = make_kuid(inode->i_sb->s_user_ns, uid);
}

static inline void i_gid_write(struct inode *inode, gid_t gid)
{
	//inode->i_gid = make_kgid(inode->i_sb->s_user_ns, gid);
}


ssize_t generic_file_splice_read(struct file *filp, loff_t *off,
		struct pipe_inode_info *piinfo, size_t sz, unsigned int ui);

int generic_file_readonly_mmap(struct file *filp, struct vm_area_struct *vma);

ssize_t generic_file_write_iter(struct kiocb *kio, struct iov_iter *iov);

ssize_t generic_file_read_iter(struct kiocb *kio, struct iov_iter *iov);

int generic_file_open(struct inode *inode, struct file *filp);

int file_write_and_wait_range(struct file *file, loff_t start, loff_t end);

const char *simple_get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *dc);

void truncate_setsize(struct inode *inode, loff_t newsize);

void truncate_inode_pages_final(struct address_space *addr_space);

typedef struct {
	long	val[2];
} __kernel_fsid_t;

struct kstatfs {
	long f_type;
	long f_bsize;
	u64 f_blocks;
	u64 f_bfree;
	u64 f_bavail;
	u64 f_files;
	u64 f_ffree;
	__kernel_fsid_t f_fsid;
	long f_namelen;
	long f_frsize;
	long f_flags;
	long f_spare[4];
};

#endif /* _OS_PHOENIX_FS_H_ */
