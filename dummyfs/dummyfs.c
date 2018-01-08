/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * dummyfs
 *
 * Copyright 2012, 2016, 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Jacek Popko, Katarzyna Baranowska, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/threads.h>
#include <sys/msg.h>
#include <sys/list.h>
#include <unistd.h>

#include "dummyfs.h"
#include "object.h"


struct {
	handle_t mutex;
} dummyfs_common;

extern int dir_find(dummyfs_object_t *dir, const char *name, oid_t *res);
extern int dir_add(dummyfs_object_t *dir, const char *name, oid_t *oid);
extern int dir_remove(dummyfs_object_t *dir, const char *name);

int dummyfs_lookup(oid_t *dir, const char *name, oid_t *res)
{
	dummyfs_object_t *d;
	int err;

	mutexLock(dummyfs_common.mutex);

	if ((d = object_get(dir->id)) == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (d->type != otDir) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	err = dir_find(d, name, res);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);

	return err;
}

int dummyfs_setattr(oid_t *oid, int type, int attr)
{
	dummyfs_object_t *o;
	int ret = EOK;

	mutexLock(dummyfs_common.mutex);

	if ((o = object_get(oid->id)) == NULL)
		return -ENOENT;

	switch (type) {

		case (atUid):
			o->uid = attr;
			break;

		case (atGid):
			o->gid = attr;
			break;

		case (atMode):
			o->mode = attr;
			break;

		case (atSize):
			ret = dummyfs_truncate(o, attr);
			break;
	}

	object_put(o);
	mutexUnlock(dummyfs_common.mutex);

	return ret;
}

int dummyfs_getattr(oid_t *oid, int type, int *attr)
{
	dummyfs_object_t *o;

	mutexLock(dummyfs_common.mutex);

	if ((o = object_get(oid->id)) == NULL)
		return -ENOENT;

	switch (type) {

		case (atUid):
			*attr = o->uid;
			break;

		case (atGid):
			*attr = o->gid;
			break;

		case (atMode):
			*attr = o->mode;
			break;

		case (atSize):
			*attr = o->size;
			break;
	}

	object_put(o);
	mutexUnlock(dummyfs_common.mutex);

	return EOK;
}

int dummyfs_link(oid_t *dir, const char *name, oid_t *oid)
{
	dummyfs_object_t *d, *o;

	if (name == NULL)
		return -EINVAL;

	mutexLock(dummyfs_common.mutex);

	if ((d = object_get(dir->id)) == NULL)
		return -ENOENT;

	if ((o = object_get(oid->id)) == NULL) {
		object_put(d);
		return -ENOENT;
	}

	if (d->type != otDir) {
		object_put(o);
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	if (o->type == otDir && o->refs > 1) {
		object_put(o);
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	dir_add(d, name, oid);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);

	return EOK;
}


int dummyfs_unlink(oid_t *dir, const char *name)
{
	int ret;
	oid_t oid;
	dummyfs_object_t *o, *d;

	dummyfs_lookup(dir, name, &oid);

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);
	o = object_get(oid.id);

	if (o == NULL) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (o->type == otDir && o->entries != NULL) {
		object_put(d);
		object_put(o);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	object_put(o);
	if ((ret = object_destroy(o)) == EOK) {
		dummyfs_truncate(o, 0);
		free(o);
	}

	dir_remove(d, name);

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);

	return ret;
}

/*
int dummyfs_mknod(oid_t *dir, const char *name, unsigned int mode, dev_t dev)
{
	dummyfs_entry_t *entry;
	dummyfs_entry_t *dirent;
	unsigned int type;

	if (dir == NULL)
		return -EINVAL;
	if (dir->type != vnodeDirectory)
		return -EINVAL;
	if (name == NULL)
		return -EINVAL;

	dirent = (dummyfs_entry_t *)dir->fs_priv;
	assert(dirent != NULL);
	proc_mutexLock(&dirent->lock);
    if (S_ISCHR(mode) || S_ISBLK(mode)) {
        type = vnodeDevice;
    } else if (S_ISFIFO(mode)) {
        type = vnodePipe;
    } else {
		proc_mutexUnlock(&dirent->lock);
        return -EINVAL;
    }

	if ((entry = _dummyfs_newentry(dir->fs_priv, name, NULL)) == NULL) {
		proc_mutexUnlock(&dirent->lock);
		return -ENOMEM;
	}

    entry->type = type;
	entry->dev = dev;
    entry->mode = mode & S_IRWXUGO;
	proc_mutexUnlock(&dirent->lock);
	return EOK;
}
*/

int dummyfs_mkdir(oid_t *dir, const char *name, int mode)
{
	dummyfs_object_t *d, *o;
	oid_t oid;
	unsigned int id;
	int ret = EOK;

	if (dir == NULL)
	   return -EINVAL;

	if (name == NULL)
		return -EINVAL;

	if (dummyfs_lookup(dir, name, &oid) == EOK)
		return -EEXIST;

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);

	o = object_create(NULL, &id);

	o->mode = mode;
	o->type = otDir;

	dir_add(d, name, &o->oid);

	object_put(d);

	mutexUnlock(dummyfs_common.mutex);

	return ret;
}


int dummyfs_rmdir(oid_t *dir, const char *name)
{

	dummyfs_object_t *d, *o;
	oid_t oid;
	int ret = EOK;

	if (dir == NULL)
		return -EINVAL;

	if (name == NULL)
		return -EINVAL;

	dummyfs_lookup(dir, name, &oid);

	mutexLock(dummyfs_common.mutex);

	d = object_get(dir->id);
	o = object_get(oid.id);

	if (o == NULL) {
		mutexUnlock(dummyfs_common.mutex);
		return -ENOENT;
	}

	if (o->type != otDir) {
		object_put(d);
		mutexUnlock(dummyfs_common.mutex);
		return -EINVAL;
	}

	if (o->entries != NULL)
		return -EBUSY;

	if ((ret = dir_remove(d, name)) == EOK) {
		object_put(o);
		if((ret = object_destroy(o)) == EOK)
			free(o);
	}

	object_put(d);
	mutexUnlock(dummyfs_common.mutex);
	return ret;
}


int dummyfs_readdir(oid_t *oid, offs_t offs, dirent_t *dirent, unsigned int count)
{
	dummyfs_entry_t *ei;

	dirent_t *bdent = 0;
	u32 dir_offset = 0;
	u32 dirent_offs = 0;
	u32 item = 0;
	u32 u_4;

	o = dummyfs_get(oid);

	if (o->type != otDir)
		return -ENOTDIR;

	if ((dirent = o->entries) == NULL)
		return -EINVAL;

	dirent = o->entries;

	do {
		item = strlen(ei->name) + 1;

		u_4=(4 - (sizeof(dirent_t) + item) % 4) % 4;
		if(dir_offset >= offs){
			if ((dirent_offs + sizeof(dirent_t) + item) > count) goto quit;
			bdent = (dirent_t*) (((char*)dirent) + dirent_offs);
			bdent->d_ino = (addr_t)&ei;
			bdent->d_off = dirent_offs + sizeof(dirent_t) + item + u_4;
			bdent->d_reclen = sizeof(dirent_t) + item + u_4;
			memcpy(&(bdent->d_name[0]), ei->name, item);
			dirent_offs += sizeof(dirent_t) + item + u_4;
		}
		dir_offset += sizeof(dirent_t) + item + u_4;
		ei = ei->next;

	}while (ei != ((dummyfs_entry_t *)vnode->fs_priv)->entries);

	return 	dirent_offs;;

quit:
	if(dirent_offs == 0)
		return  -EINVAL; /* Result buffer is too small */

	return dirent_offs;

}

/*

int dummyfs_ioctl(file_t* file, unsigned int cmd, unsigned long arg)
{

	//TODO
	return -ENOENT;
}


int dummyfs_open(oid_t *file)
{
	if (file == NULL)
		return -EINVAL;
	if (vnode->type != vnodeFile)
		return -EINVAL;

	file->priv = (dummyfs_entry_t *) vnode->fs_priv;
	assert(file->priv != NULL);
	return EOK;
}


int dummyfs_fsync(file_t* file)
{
	dummyfs_entry_t *entry;

	if (file == NULL || file->vnode == NULL || file->vnode->type != vnodeFile)
		return -EINVAL;
	entry = file->priv;
	assert(entry != NULL);

	return EOK;
}

*/

int main(void)
{
	u32 port;
	oid_t toid;
	msg_t msg;
	unsigned int rid;
	
	usleep(500000);
	portCreate(&port);
	printf("dummyfs: Starting dummyfs server at port %d\n", port);

	/* Try to mount fs as root */
	if (portRegister(port, "/", &toid) == EOK)
		printf("dummyfs: Mounted as root %s\n", "");

	mutexCreate(&dummyfs_common.mutex);

	/* Create root directory */


	for (;;) {
		msgRecv(port, &msg, &rid);

		switch (msg.type) {
		case mtOpen:
			break;
		case mtWrite:
//			msg.o.io.err = dummyfs_write(&msg.i.io.oid, msg.i.data, msg.i.size);
			break;
		case mtRead:
			msg.o.io.err = 0;
			msg.o.size = 1;
//			msg.o.io.err = dummyfs_read(&msg.i.io.oid, msg.o.data, msg.o.size);
			break;
		case mtClose:
			break;
		}

		msgRespond(port, rid);
	}

	return EOK;
}
