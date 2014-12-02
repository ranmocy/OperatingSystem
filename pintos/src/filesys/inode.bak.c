#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

#define NULL_SECTOR -1

#define INODE_MAGIC 0x494e4f44
#define TREE_ORDER ((BLOCK_SECTOR_SIZE - 4 - 2*sizeof(block_sector_t) - sizeof(unsigned))/sizeof(struct b_tree_record))
#define LAST_ELEM (TREE_ORDER - 1)
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
} ;

/* returns the index of the last element which <= off , or -1 if no such an element exists.*/
int16_t b_tree_search_node(struct b_tree_node* ptr, off_t off){
	int min, max, mid;
	min = 0; max = ptr->nkey;
	if (min == max || ptr->data[0].off > off)
		return -1;
	while (true){
		if (ptr->data[min].off == off || min + 1 == max)
			return min;
		mid = (min + max) / 2;
		if (ptr->data[mid].off > off)
			max = mid;
		else
			min = mid;
	}
}

block_sector_t b_tree_find(block_sector_t root, off_t off){
	struct b_tree_node* ptr;

	ptr = (struct b_tree_node*)cache_get_ptr_ro(root);
	if (ptr->data[0].off > off)
		return NULL_SECTOR;
	while (true){
		int16_t t = b_tree_search_node(ptr, off);
		ASSERT(t >= 0);
		if (!ptr->height)
			return (ptr->data[t].off == off)?ptr->data[t].sector:NULL_SECTOR;
		root = ptr->data[t].sector;
		cache_release_ptr(ptr);
		ptr = (struct b_tree_node*)cache_get_ptr_ro(root);
	}
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

enum insert_result{
	INS_SUCCESS,
	INS_FAIL,
	INS_FATAL
}

void shift_right(struct b_tree_node* ptr, int16_t loc, off_t off, block_sector_t sector){
	if (loc == 0){
		memmove(ptr->data + 1, ptr->data, min(LAST_ELEM, ptr->nkey)  * sizeof(struct b_tree_record));
		b_tree_update_first(ptr, off);
		ptr->data[0].sector = sector;
	}else{
		memmove(ptr->data + loc + 1, ptr->data + loc, (min(LAST_ELEM, ptr->nkey) - loc) * sizeof(struct b_tree_record))
		ptr->data[loc].off = off;
		ptr->data[loc].sector = sector;
	}
}

inline void set_parent(block_sector_t parent, block_sector_t child){
	struct b_tree_node* childptr = (struct b_tree_node*)cache_get_ptr_rw(child);
	childptr->parent = parent;
	cache_release_ptr(childptr);
}

inline void move_child(struct b_tree_block* src,
						int16_t src_off,
						struct b_tree_block* dst,
						block_sector_t dst_sector.
						int16_t dst_off,
						int16_t size){
	int i;
	ASSERT(src->height == dst->height);
	if (src != dst && src->height)
		for (i = 0; i < size; i++)
			set_parent(dst_sector, src->data[src_off + i].sector);
	memmove(dst->data+dst_off, src->data+src_off, size * sizeof(struct b_tree_record));
}

bool b_tree_insert(struct b_tree_node* ptr, off_t off, block_sector_t sector, block_sector_t root){
	int16_t t = b_tree_search_node(ptr, off);
	struct b_tree_node * next = NULL;

	ASSERT(t < 0 || ptr->offs[t] != off);

	if (ptr->nkey < TREE_ORDER){
		shift_right(ptr, t + 1, off, sector);
		ptr->nkey++;
		return true;
	}else if (ptr->parent == NULL_SECTOR){
		// It's root node
		bool res;
		block_sector_t a,b;
		struct b_tree_node *aptr, *bptr;
		int16_t sep = TREE_ORDER / 2;
		if (!free_map_allocate(1, &a))
			return false;
		if (!free_map_allocate(1, &b)){
			free_map_release(a, 1);
			return false;
		}
		aptr = (struct b_tree_node*)cache_get_ptr_rw(a);
		bptr = (struct b_tree_node*)cache_get_ptr_rw(b);

		aptr->height = bptr->height = ptr->height;
		aptr->nkey = sep;
		bptr->nkey = TREE_ORDER - sep;
		aptr->next = b;
		bptr->next = NULL_SECTOR;
		aptr->parent = bptr->parent = root;
		move_child(ptr, 0, aptr, a, 0, aptr->nkey);
		move_child(ptr, sep, bptr, b, 0, bptr->nkey);

		ptr->height++;
		ptr->nkey = 2;
		ptr->data[0].off = aptr->data[0].off;
		ptr->data[0].sector = a;
		ptr->data[1].off = bptr->data[0].off;
		ptr->data[1].sector = b;
		
		// TODO res =  retry in aptr and bptr
		cache_release_ptr(bptr);
		cache_release_ptr(aptr);
		return res;
	}else if(ptr->next == NULL_SECTOR){
		// if next node is not exist.
		bool res;
		struct b_tree_node *next, *par;
		block_sector_t next_sector;
		int16_t sep = TREE_ORDER / 2;
		if (!free_map_allocate(1, &next_sector)
			return false;
		next = (struct b_tree_node*)cache_get_ptr_rw(ptr->next);
		next->height = ptr->height;
		next->nkey = TREE_ORDER - sep;
		next->next = NULL_SECTOR;
		next->parent = ptr->parent;

		par = (struct b_tree_node*)cache_get_ptr_rw(ptr->parent);
		move_child(ptr, sep, next, ptr->next, 0, next->nkey);
		if (!b_tree_insert(par, next->data[0].off, ptr->next, root)){
			cache_release_ptr(par);
			cache_release_ptr(next);
			free_map_release(next_sector);
			return false;
		}
		ptr->nkey = sep;
		ptr->next = next_sector;
		// TODO res = retry in ptr and next
		cache_release_ptr(par);
		cache_release_ptr(next);
		return res;
	}else if ((next=(struct b_tree_node*) cache_get_ptr_rw(ptr->next))->nkey < TREE_ORDER){
		// if exist next node and it's not full.
		if (loc == TREE_ORDER){
			shift_right(next, 0, off, record);
			if (ptr->height)
				set_parent(ptr->next, sector);
		}else{
			shift_right(next, 0, ptr->data[LAST_ELEM].off, ptr->data[LAST_ELEM].sector);
			next->data[0] = ptr->data[LAST_ELEM];
			if (ptr->height)
				set_parent(ptr->next, next->data[0].sector)
			shift_right(ptr, loc, off, sector);
		}
		next->nkey++;
		cache_release_ptr(next);
		return true;
	}else{
		// both this and next node is full.
		bool res;
		off_t old_off;
		struct b_tree_node *mid, par;
		block_sector_t mid_sector;
		int16_t sep1 = TREE_ORDER *2 / 3, sep2 = TREE_ORDER * 4 / 3;
		if (!free_map_allocate(1, &mid_sector))
			return false;
		mid = (struct b_tree_node*)cache_get_ptr_rw(mid_sector);
		mid->height = ptr->height;
		mid->nkey = sep2 - sep1;
		mid->next = ptr->next;
		mid->parent = ptr->parent;

		move_child(ptr, sep1, mid, mid_sector, 0,TREE_ORDER-sep1);
		move_child(next, 0, mid, mid_sector, TREE_ORDER-sep1, sep2 - TREE_ORDER);
		par = (struct b_tree_node*)cache_get_ptr_rw(ptr->parent);
		if (!b_tree_insert(par, mid->data[0].off, mid_sector, root)){
			cache_release_ptr(par);
			cache_release_ptr(mid);
			cache_release_ptr(next);
			free_map_allocate(mid_sector, 1);
			return false;
		}
		ptr->nkey = sep1;
		ptr->next = mid_sector;

		next->nkey =  2*TREE_ORDER - sep2;
		next->data[0].off;
		b_tree_update_first(next, next->data[sep2-TREE_ORDER].off);
		move_child(next, sep2-TREE_ORDER, next, mid->next, 0, next->nkey);
		// TODO res = retry in ptr, mid and next
		cache_release_ptr(par);
		cache_release_ptr(mid);
		cache_release_ptr(next);
		return res;
	}
}

void b_tree_remove(block_sector_t sector){
	struct b_tree_node* ptr = (struct b_tree_node*)cache_get_ptr_ro(sector);
	int i;
	if (ptr->height)
		for (i = 0; i < ptr->nkey; i++)
			b_tree_remove(ptr->data[i].sector);
	else
		for (i = 0; i < ptr->nkey; i++)
			free_map_release(ptr->data[i].sector, 1);
	cache_release_ptr(ptr);
	free_map_release(sector ,1);
}


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
	struct b_tree_node* ptr = (struct b_tree_node*)cache_get_ptr_rw(sector);
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

off_t inode_write_at(struct inode *inode, void *buffer, off_t size, off_t offset){

}

void inode_deny_write(struct inode *inode){
	inode->deny_write_cnt++;
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

void inode_allow_write(struct inode *inode){
	ASSERT(inode->deny_write_cnt >0);
	ASSERT(inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

off_t inode_length(const struct inode* inode){
	
}