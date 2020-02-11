/*
 * Phoenix-RTOS
 *
 * ext2
 *
 * object.c
 *
 * Copyright 2017 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/rb.h>
#include <sys/file.h>
#include <sys/threads.h>
#include <sys/list.h>

#include "ext2.h"
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "object.h"


static int object_cmp(rbnode_t *n1, rbnode_t *n2)
{
	ext2_object_t *o1 = lib_treeof(ext2_object_t, node, n1);
	ext2_object_t *o2 = lib_treeof(ext2_object_t, node, n2);

	if (o1->id > o2->id)
		return 1;
	else if (o1->id < o2->id)
		return -1;

	return 0;
}


static int object_destroy_unlocked(ext2_object_t *o)
{
	ext2_object_t t;

	t.id = o->id;

	if ((o == lib_treeof(ext2_object_t, node, lib_rbFind(&o->f->objects->used, &t.node)))) {
		if (o->f->objects->used_cnt)
			o->f->objects->used_cnt--;
		else
			debug("ext2: GLOBAL OBJECT COUNTER UNDERFLOW\n");
		lib_rbRemove(&o->f->objects->used, &o->node);
	}

	inode_free(o->f, o->id, o->inode);
	free(o->ind[0].data);
	free(o->ind[1].data);
	free(o->ind[2].data);
	/*mutex destroy */
	resourceDestroy(o->lock);
	free(o);
	return EOK;
}


int object_destroy(ext2_object_t *o)
{
	if (!o)
		return -EINVAL;

	ext2_fs_objects_t *objects = o->f->objects;

	mutexLock(objects->ulock);
	object_destroy_unlocked(o);
	mutexUnlock(objects->ulock);

	return EOK;
}


int object_remove(ext2_object_t *o)
{
	if (!o)
		return EOK;

	lib_rbRemove(&o->f->objects->used, &o->node);
	if (o->f->objects->used_cnt)
		o->f->objects->used_cnt--;
	else
		debug("ext2: GLOBAL OBJECT COUNTER UNDERFLOW\n");

	object_sync(o);
	inode_put(o->inode);
	free(o->ind[0].data);
	free(o->ind[1].data);
	free(o->ind[2].data);
	/*mutex destroy */
	resourceDestroy(o->lock);
	free(o);

	return EOK;
}


ext2_object_t *object_create(ext2_fs_info_t *f, id_t *id, id_t *pid, ext2_inode_t **inode, int mode)
{
	ext2_object_t *o, t;

	mutexLock(f->objects->ulock);

	if (*inode == NULL) {
		*inode = calloc(1, f->inode_size);

		*id = inode_create(f, *inode, mode, *pid);

		if (!*id) {
			free(*inode);
			*inode = NULL;
			return NULL;
		}
	}
	t.id = *id;

	if ((o = lib_treeof(ext2_object_t, node, lib_rbFind(&f->objects->used, &t.node))) != NULL) {
		o->refs++;
		mutexUnlock(f->objects->ulock);
		return o;
	}

	if (f->objects->used_cnt >= EXT2_MAX_FILES) {
		// TODO: free somebody from lru
		if (!f->objects->lru) {
			debug("ext2: max files reached, lru is empty no space to free\n");
			/* TODO: fix inode possible leak */
			inode_free(o->f, *id, *inode);
			return NULL;
		}
		o = f->objects->lru;
		LIST_REMOVE(&f->objects->lru, o);
		object_remove(o);
		debug("ext2 max files reached removing\n");
	}

	o = (ext2_object_t *)malloc(sizeof(ext2_object_t));
	if (o == NULL) {
		mutexUnlock(f->objects->ulock);
		return NULL;
	}

	memset(o, 0, sizeof(ext2_object_t));
	o->refs = 1;
	o->id = *id;
	o->inode = *inode;
	object_setFlag(o, EXT2_FL_DIRTY);
	mutexCreate(&o->lock);
	o->f = f;

	o->next = NULL;
	o->prev = NULL;
	lib_rbInsert(&f->objects->used, &o->node);
	f->objects->used_cnt++;

	mutexUnlock(f->objects->ulock);

	return o;
}


ext2_object_t *object_get(ext2_fs_info_t *f, id_t *id)
{
	ext2_object_t *o, t;
	ext2_inode_t *inode = NULL;

	t.id = *id;

	mutexLock(f->objects->ulock);
	/* check used/opened inodes tree */
	if ((o = lib_treeof(ext2_object_t, node, lib_rbFind(&f->objects->used, &t.node))) != NULL) {
		if (!o->refs)
			LIST_REMOVE(&f->objects->lru, o);
		o->refs++;
		mutexUnlock(f->objects->ulock);
		return o;
	}
	inode = inode_get(f, *id);

	if (inode != NULL) {

		mutexUnlock(f->objects->ulock);
		o = object_create(f, id, NULL, &inode, inode->mode);
		return o;
	}
	mutexUnlock(f->objects->ulock);

	return o;
}


void object_sync(ext2_object_t *o)
{

	if (object_checkFlag(o, EXT2_FL_DIRTY))
		inode_set(o->f, o->id, o->inode);

	object_clearFlag(o, EXT2_FL_DIRTY);

	if (object_checkFlag(o, EXT2_FL_MOUNT))
		return;

	if (o->ind[0].data)
		write_block(o->f, o->ind[0].bno, o->ind[0].data);
	if (o->ind[0].data)
		write_block(o->f, o->ind[1].bno, o->ind[1].data);
	if (o->ind[0].data)
		write_block(o->f, o->ind[2].bno, o->ind[2].data);
}


void object_put(ext2_object_t *o)
{
	char buf[64];

	if (!o)
		return;

	mutexLock(o->f->objects->ulock);
	if (o->refs)
		o->refs--;
	else {
		sprintf(buf, "ext2: REF UNDERFLOW %lld\n", o->id);
		debug(buf);
	}

	if(!o->inode->nlink && !o->refs) {
		object_destroy_unlocked(o);
		mutexUnlock(o->f->objects->ulock);
		return;
	}

	if (!o->refs)
		LIST_ADD(&o->f->objects->lru, o);

	mutexUnlock(o->f->objects->ulock);

	return;
}


int object_init(ext2_fs_info_t *f)
{
	f->objects = calloc(1, sizeof(ext2_fs_objects_t));
	if(!f->objects)
		return -ENOMEM;

	lib_rbInit(&f->objects->used, object_cmp, NULL);

	f->objects->used_cnt = 0;
	f->objects->lru = NULL;

	mutexCreate(&f->objects->ulock);
	return EOK;
}
