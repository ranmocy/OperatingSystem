#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

#define NULL_SECTOR -1

#define TREE_ORDER ((BLOCK_SECTOR_SIZE - 4 - 2*sizeof(block_sector_t) - sizeof(unsigned))/sizeof(struct b_tree_record))
#define LAST_ELEM (TREE_ORDER - 1)

#define LEAF_LEN (TREE_ORDER * sizeof(b_tree_record) / (sizeof(b_tree_record) + sizeof(size_t)))
#define LEAF_LAST_ELEM (LEAF_LEN - 1)
struct b_tree_record{
	off_t off;
	block_sector_t sector;
};

struct b_tree_node{
	uint16_t height;
	int16_t nkey;
	block_sector_t next;
	block_sector_t parent;		// for root node, this is always NULL_SECTOR
	unsigned magic;
	struct b_tree_record data[TREE_ORDER];
};

struct b_tree_leaf{
	uint16_t height;			// this will always be 0;
	int16_t nkey;
	block_sector_t next;
	block_sector_t parent;
	unsigned magic;
	struct b_tree_record data[LEAF_LEN];
	size_t size[LEAF_LEN];
}

// find the first element greater than off
int16_t b_tree_search_node(struct b_tree_node* ptr, off_t off){
	int min, max, mid;
	min = 0; max = ptr->nkey;
	if (min == max || ptr->data[0].off > off)
		return 0;
	while (true){
		if (ptr->data[min].off == off || min + 1 == max)
			return min + 1;
		mid = (min + max) / 2;
		if (ptr->data[mid].off > off)
			max = mid;
		else
			min = mid;
	}
}


block_sector_t set_parent(block_sector_t sector, block_sector_t parent){
	struct b_tree_node* ptr = (struct b_tree_node*)cache_get_ptr_rw(sector);
	block_sector_t ret = ptr->parent;
	ptr->parent = parent;
	cache_release_ptr(ptr);
	return ret;
}

block_sector_t move_child(struct b_tree_node* src, 
						int16_t srcoff, 
						struct b_tree_node* dst, 
						block_sector_t dst_sector,
						int16_t dstoff,
						int16_t size){
	block_sector_t ret = NULL_SECTOR;
	int16_t i;
	ASSERT(src->height == dst->height);
	if (src->height == 0){
		struct b_tree_leaf *lsrc = (struct b_tree_leaf*)src;
		struct b_tree_leaf *ldst = (struct b_tree_leaf*)dst;
		memmove(ldst->data + dstoff, lsrc->data + srcoff, size * sizeof(b_tree_record));
		memmove(ldst->size + dstoff, lsrc->size + srcoff, size * sizeof(size_t));
	}else{
		for(i = 0 ; i < size; i++)
			ret = set_parent(src->data[i].sector, dst_sector)
		memmove(ldst->data + dstoff, lsrc->data + srcoff, size * sizeof(b_tree_record));
	}	
	return ret;
}

void b_tree_update_first(struct b_tree_node* ptr, off_t new_off){
	off_t old_off = ptr->data[0].off
	block_sector_t sector;

	ptr->data[0].off = new_off;
	sector = ptr->parent;
	while (sector != NULL_SECTOR){
		int16_t t;
		ptr = (struct b_tree_node*) cache_get_ptr_rw(sector);
		t = b_tree_search_node(ptr, old_off);
		ASSERT(t >= 0 && ptr->data[t].off == old_off);
		ptr->data[t].off = new_off;
		sector = ptr->parent;
		cache_release_ptr(ptr);
		if (t) break;
	}
}

void b_tree_node_add(struct b_tree_node* ptr, int16_t loc, off_t off, block_sector_t sector){
	ASSERT(ptr->nkey < TREE_ORDER);
	memmove(ptr->data + loc + 1, ptr->data + loc, (ptr->nkey - loc) * sizeof(struct b_tree_record));
	if (loc == 0)
		b_tree_update_first(ptr, off);
	else
		ptr->data[loc].off = off;
	ptr->data[loc].sector = sector;
	ptr->nkey++;
}

void b_tree_leaf_add(struct b_tree_leaf *ptr, int16_t loc, off_t off, block_sector_t sector, size_t size){
	ASSERT(ptr->nkey < LEAF_LEN)
	b_tree_node_add((struct b_tree_node*)ptr, loc, off, sector);
	memmove(ptr->size + loc + 1, ptr->size + loc, (ptr->nkey - loc - 1)*sizeof(size_t));
}

bool b_tree_split_root(struct b_tree_node *ptr, int16_t size, block_sector_t root_sector, int16_t *sep){
	block_sector_t a,b;
	struct b_tree_node *aptr, *bptr;

	ASSERT(ptr->parent = NULL_SECTOR);
	*sep = size / 2;
	if (!free_map_allocate(1,&a))
		return false;
	if (!free_map_allocate(1,&b)){
		free_map_release(a,1);
		return false;
	}
	aptr = (struct b_tree_node*)cache_get_ptr_rw(a);
	bptr = (struct b_tree_node*)cache_get_ptr_rw(b);

	aptr->height = bptr->height = ptr->height;
	aptr->nkey = *sep;
	bptr->nkey = size - *sep;
	aptr->next = b;
	bptr->next = NULL_SECTOR;
	aptr->parent = bptr->parent = root_sector;
	move_child(ptr, 0, aptr, a, 0, aptr->nkey);
	move_child(ptr, *sep, bptr, b, 0, bptr->nkey);

	ptr->height++;
	ptr->nkey = 2;
	ptr->data[0].off = aptr->data[0].off;
	ptr->data[0].sector = a;
	ptr->data[1].sector = b;

	cache_release_ptr(bptr);
	cache_release_ptr(aptr);
	return true;
}

bool b_tree_split_next(struct b_tree_node* ptr, int16_t size, int16_t *sep){
	struct b_tree_node *next, *par;
	block_sector_t next_sector, ptr_sector;

	ASSERT(ptr->next == NULL_SECTOR);
	*sep = size / 2;
	if (!free_map_allocate(1, &next_sector))
		return false;
	next = (struct b_tree_node*)cache_get_ptr_rw(next_sector);
	next->height = ptr->height;
	next->nkey = size - *sep;
	next->next = NULL_SECTOR;
	next->parent = ptr->parent;

	par = (struct b_tree_node*)cache_get_ptr_rw(ptr->parent);
	ptr_sector = move_child(ptr, *sep, next, next_sector, 0, next->nkey);
	if (!b_tree_insert_node(par,next->data[0])){
		if (ptr_sector != NULL_SECTOR)
			move_child(next, 0, ptr, ptr_sector, *sep, next->nkey);
		cache_release_ptr(par);
		cache_release_ptr(next)
		free_map_release(next_sector, 1);
		return false;
	}

	ptr->nkey = *sep;
	ptr->next = next_sector;
	cache_release_ptr(par);
	cache_release_ptr(next);
	return true;
}

bool b_tree_split_node(struct b_tree_node* aptr, struct b_tree_node* bptr, int16_t size, int16_t *sep1){
	struct b_tree_node *cptr, par;
	block_sector_t a, b, c;
	int16_t sep2;

	ASSERT(aptr->nkey == size && bptr->nkey == size);
	*sep1 = size*2/3;
	sep2 = size*4/3;
	if (!free_map_allocate(1,&c))
		return false;
	cptr = (struct b_tree_node*)cache_get_ptr_rw(c);
	cptr->height = ptr->height;
	cptr->nkey = 2*size-sep2;
	cptr->next = bptr->next;
	cptr->parent = bptr->parent;

	b = move_child(bptr, sep2-size, cptr, c, 0, cptr->nkey);
	par = (struct b_tree_node*)cache_get_ptr_rw(cptr->parent);
	if (!b_tree_insert_node(par, cptr->data[0].off, c, root_sector)){
		if (b != NULL_SECTOR)
			move_child(cptr, 0, bptr, b, sep2-size, cptr->nkey);
		cache_release_ptr(par);
		cache_release_ptr(cptr);
		free_map_release(c);
		return false;
	}
	move_child(bptr, 0, bptr, b, size-*sep1, sep2-size);
	move_child(aptr, sep1, bptr, b, 0, size-*sep1);
	aptr->nkey = *sep1;
	bptr->nkey = sep2 - *sep1;
	bptr->next = c;
	cache_release_ptr(par);
	cache_release_ptr(cptr);
	return true;
}

bool b_tree_insert_leaf(struct b_tree_leaf* ptr,
						off_t off,
						block_sector_t sector,
						size_t size,
						block_sector_t root){
	struct b_tree_node* nptr = (struct b_tree_node*)ptr;
	int16_t t = b_tree_search_node(nptr, off);
	struct b_tree_node * next = NULL;

	// boundary check
	ASSERT(t == LEAF_LEN || off+size <= ptr->data[t].off);
	ASSERT(t == 0 || ptr->data[t-1].off + ptr->size[t-1] <= off);

	if (ptr->nkey < LEAF_LEN){
		// There are free space there!
		b_tree_leaf_add(ptr, t, off, sector, size);
		return true;
	}else if (ptr->parent == NULL_SECTOR){
		// if ptr is root node
		int16_t sep;
		if (!b_tree_split_root(nptr, LEAF_LEN, root, &sep))
			return false;
		if (t >= sep){
			ptr = (struct b_tree_leaf*)cache_get_ptr_rw(ptr->data[1].sector);
			t -= sep
		}else
			ptr = (struct b_tree_leaf*)cache_get_ptr_rw(ptr->data[0].sector);
		b_tree_leaf_add(ptr, t, off, sector, size);
		cache_release_ptr(ptr);
		return true;
	}else if (ptr->next == NULL_SECTOR){
		// if next node is not exist.
		int16_t sep;
		if (!b_tree_split_next(ptr, LEAF_LEN, &sep))
			return false;

		if (t >= sep){
			next = (struct b_tree_leaf*)cache_get_ptr_rw(ptr->next);
			t -= sep;
			b_tree_leaf_add(next, t, off, sector, size);
			cache_release_ptr(next);
		}else
			b_tree_leaf_add(ptr, t, off, sector, size);
		return true;
	}else if ((next=(struct b_tree_leaf*)cache_get_ptr_rw(ptr->next))->nkey < LEAF_LEN){
		// if exist next node and it has free space
		if (t == LEAF_LEN)
			b_tree_leaf_add(next, 0, off, sector, size);
		else{
			b_tree_leaf_add(next, 0, ptr->data[LEAF_LAST_ELEM].off, ptr->data[LEAF_LAST_ELEM].sector, ptr->size[LEAF_LAST_ELEM]);
			ptr->nkey--;
			b_tree_leaf_add(ptr, t, off, sector, size);
		}
		cache_release_ptr(next);
		return true;
	}else{
		// both this and next node are full.
		int16_t sep;
		if (!b_tree_split_node(nptr, next, LEAF_LEN, &sep)){
			cache_release_ptr(next);
			return false;
		}
		if (t >= sep){
			t-=sep;
			b_tree_leaf_add(next, t, off ,sector,size);
		}else
			b_tree_leaf_add(ptr, t, off, sector, size);
		cache_release_ptr(next);
		return true;
	}
}

bool b_tree_insert_node(struct b_tree_node* ptr, 
				off_t off, 
				block_sector_t sector, 
				block_sector_t root){
	int16_t t = b_tree_search_node(ptr, off) + 1;
	struct b_tree_node * next = NULL;

	ASSERT(t == 0 || ptr->offs[t-1] != off);

	if (ptr->nkey < TREE_ORDER){
		// There are free space there!
		b_tree_node_add(ptr, t, off, sector);
		return true;
	}else if (ptr->parent == NULL_SECTOR){
		// if ptr is root node
		int16_t sep;
		block_sector_t newpar_sector;
		if (!b_tree_split_root(ptr, TREE_ORDER, root, &sep))
			return false;
		if (t >= sep){
			newpar_sector = ptr->data[1].sector;
			t -= sep;
		}else
			newpar_sector = ptr->data[0].sector;
		ptr = (struct b_tree_node*)cache_get_ptr_rw(newpar_sector);
		set_parent(sector, newpar_sector);
		b_tree_node_add(ptr, t, off, sector);
		cache_release_ptr(ptr);
		return true;
	}else if(ptr->next == NULL_SECTOR){
		// if next node is not exist.
		int16_t sep;
		if (!b_tree_split_next(ptr, TREE_ORDER, &sep))
			return false;
		if (t >= sep){
			next = (struct b_tree_node*)cache_get_ptr_rw(ptr->next);
			t-=sep;
			set_parent(sector, ptr->next);
			b_tree_node_add(next, t, off, sector);
			cache_release_ptr(next);
		}else
			b_tree_node_add(ptr, t, off, sector);
		return true;
	}else if ((next=(struct b_tree_node*) cache_get_ptr_rw(ptr->next))->nkey < TREE_ORDER){
		// if exist next node and it has free space
		if (t == TREE_ORDER){
			set_parent(sector, ptr->next);
			b_tree_node_add(next, 0, off, sector);
		}else{
			struct b_tree_record *swap = ptr->data + LAST_ELEM;
			set_parent(swap->sector, ptr->next);
			b_tree_node_add(next, 0, swap->off, swap->sector);
			ptr->nkey--;
			b_tree_node_add(ptr, t, off, sector);
		}
		cache_release_ptr(next);
		return true;
	}else{
		// both this and next node is full.
		int16_t sep;
		if (!b_tree_split_node(ptr, next, TREE_ORDER, &sep)){
			cache_release_ptr(next);
			return false;
		}
		if (t >= sep){
			t -= sep;
			b_tree_node_add(next, t, off, sector);
		}else
			b_tree_node_add(ptr, t, off ,sector);
		cache_release_ptr(next);
		return true;
	}
}

void b_tree_remove(block_sector_t sector){
	struct b_tree_node* ptr = (struct b_tree_node*)cache_get_ptr_ro(sector);
	int i;
	if (ptr->height)
		for(i = 0; i < ptr->nkey; i++)
			b_tree_remove(ptr->data[i].sector);
	else
		for(i = 0; i < ptr->nkey; i++)
			free_map_release(ptr->data[i].sector, ptr->size[i])
	cache_release_ptr(ptr);
	free_map_release(sector, 1);
}}

struct inode
{
	struct list_elem elem;			/* Element in inode list. */
	block_sector_t sector;			/* Sector number of disk location. */
	int open_cnt;					/* Number of openers. */
	bool removed;					/* True if deleted, false otherwise. */
	int deny_write_cnt;				/* 0: writes ok, >0: deny writes. */
	struct lock;					
};

static struct list open_inodes;

void inode_init(void){
	list_init(&open_inodes);
}

bool inode_create(block_sector_t sector, off_t length){
	struct b_tree_node* ptr = (struct b_tree_leaf*)cache_get_ptr_rw(sector);
	ASSERT (length >= 0);
	ASSERT (sizeof(struct b_tree_node) == BLOCK_SECTOR_SIZE);
	ptr->height = ptr->nkey = 0;
	ptr->next = ptr->parent = NULL_SECTOR;
	ptr->magic == INODE_MAGIC;
	cache_release_ptr(ptr);
}

struct inode* inode_open(blokc_sector_t sector){
	struct list_elem *e;
	struct inode *inode;

	for (e = list_begin(&open_inodes); e != list_end(&open_inodes); e = list_next(e)){
		inode = list_entry(e, struct inode, elem);
		if (inode->sector == sector){
			inode_reopen(inode);
			return inode;
		}
	}

	inode = malloc (sizeof(inode));
	if (inode == NULL)
		return NULL;

	list_push_front(&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	lock_init(&inode->lock);
	return inode;
}

struct inode* inode_reopen(struct inode* inode){
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

block_sector_t inode_get_number(const struct inode* inode){
	return inode->sector;
}

void inode_close(struct inode *inode){
	if(inode==NULL)
		return;

	if (--inode->open_cnt == 0){
		list_remove(&inode->elem);
		if (inode_removed){
			b_tree_remove(inode->sector);
		}
		free(inode);
	}
}

void inode_remove(struct inode *inode){
	ASSERT(inode != NULL);
	inode->removed = true;
}

off_t inode_read_at(struct inode *inode, void *buffer, off_t size, off_t offset){

}