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

#include "../os-phoenix.h"
#include "fs.h"


struct timespec current_time(struct inode *inode)
{
	struct timespec t = {0, 0};
	return t;
}

int setattr_prepare(struct dentry *dentry, struct iattr *iattr)
{
	return 0;
}

void init_special_inode(struct inode *inode, umode_t mode, dev_t dev)
{
	return;
}


void inc_nlink(struct inode *inode)
{
	return;
}

void clear_nlink(struct inode *inode)
{
	return;
}

void set_nlink(struct inode *inode, unsigned int nlink)
{
	return;
}

void drop_nlink(struct inode *inode)
{
	return;
}

void ihold(struct inode * inode)
{
}

struct inode *new_inode(struct super_block *sb)
{
	return NULL;
}

void unlock_new_inode(struct inode *inode)
{
}

void iget_failed(struct inode *inode)
{
}

struct inode * iget_locked(struct super_block *sb, unsigned long ino)
{
	return NULL;
}

void iput(struct inode *inode)
{
}

void clear_inode(struct inode *inode)
{
}

bool is_bad_inode(struct inode *inode)
{
	return 0;
}

struct inode *ilookup(struct super_block *sb, unsigned long ino)
{
	return NULL;
}

int insert_inode_locked(struct inode *inode)
{
	return 0;
}

void make_bad_inode(struct inode *inode)
{
}

ssize_t generic_file_splice_read(struct file *filp, loff_t *off,
		struct pipe_inode_info *piinfo, size_t sz, unsigned int ui) 
{
	return 0;
}

int generic_file_readonly_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

ssize_t generic_file_write_iter(struct kiocb *kio, struct iov_iter *iov)
{
	return 0;
}


ssize_t generic_file_read_iter(struct kiocb *kio, struct iov_iter *iov)
{
	return 0;
}


int generic_file_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int file_write_and_wait_range(struct file *file, loff_t start, loff_t end)
{
	return 0;
}

const char *simple_get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *dc)
{
	return NULL;
}

void truncate_setsize(struct inode *inode, loff_t newsize)
{
}

void truncate_inode_pages_final(struct address_space *addr_space)
{
}
