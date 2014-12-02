#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

#define TREE_ORDER ((BLOCK_SECTOR_SIZE - 4 - 2*sizeof(block_sector_t) - sizeof(unsigned))/sizeof(struct b_tree_record))
#define LAST_ELEM (TREE_ORDER - 1)

#define LEAF_LEN (TREE_ORDER * sizeof(struct b_tree_record) / (sizeof(struct b_tree_record) + sizeof(size_t)))
#define LEAF_LAST_ELEM (LEAF_LEN - 1)
typedef uint32_t sector_idx;

struct b_tree_record;
struct b_tree_node;
struct b_tree_leaf;

bool b_tree_insert_node(struct b_tree_node **ptr, uint8_t *loc, 
				sector_idx idx, 
				block_sector_t sector, 
				block_sector_t root);
// find the first element greater than idx
uint8_t b_tree_search_node(struct b_tree_node* ptr, sector_idx idx);
block_sector_t set_parent(block_sector_t sector, block_sector_t parent);
block_sector_t move_child(struct b_tree_node* src, 
						uint8_t srcoff, 
						struct b_tree_node* dst, 
						block_sector_t dst_sector, 
						uint8_t dstoff, 
						uint8_t size);
void b_tree_update_first(struct b_tree_node* ptr, sector_idx new_off);
void b_tree_node_add(struct b_tree_node* ptr, uint8_t loc, sector_idx idx, block_sector_t sector);
void b_tree_leaf_add(struct b_tree_leaf *ptr, uint8_t loc, sector_idx idx, block_sector_t sector, size_t size);
bool b_tree_split_root(struct b_tree_node *ptr, uint8_t size, block_sector_t root_sector, uint8_t *sep);
bool b_tree_split_next(struct b_tree_node* ptr, uint8_t size, uint8_t *sep, block_sector_t root);
bool b_tree_split_node(struct b_tree_node* aptr, 
					struct b_tree_node* bptr, 
					uint8_t size, 
					uint8_t *sep1, 
					block_sector_t root);
bool b_tree_insert_leaf(struct b_tree_leaf** ptr, 
						uint8_t *loc,
						sector_idx idx,
						block_sector_t sector,
						size_t size,
						block_sector_t root);
bool b_tree_insert_node(struct b_tree_node **ptr, uint8_t *loc, 
						sector_idx idx, 
						block_sector_t sector, 
						block_sector_t root);
void b_tree_remove(block_sector_t sector);

block_sector_t malloc_sector(size_t *size);
struct b_tree_iter;
bool b_tree_iter_begin(struct b_tree_iter* iter, block_sector_t root, sector_idx begin, sector_idx end);
bool b_tree_iter_next(struct b_tree_iter* iter);

inline sector_idx min(sector_idx a, sector_idx b);

inline sector_idx min(sector_idx a, sector_idx b){return (a<b)?a:b;}

struct inode;

struct b_tree_record{
	sector_idx idx;
	block_sector_t sector;
};

struct b_tree_node{
	uint8_t height, nkey, flag, __;
	block_sector_t next;
	block_sector_t parent;		// for root node, this is always NULL_SECTOR
	size_t length;
	struct b_tree_record data[TREE_ORDER];
};

struct b_tree_leaf{
	uint8_t height, nkey, flag, __;
	block_sector_t next;
	block_sector_t parent;
	size_t length;
	struct b_tree_record data[LEAF_LEN];
	size_t size[LEAF_LEN];
	char __unused__[4];
};

// find the first element greater than idx
uint8_t b_tree_search_node(struct b_tree_node* ptr, sector_idx idx){
	int min, max, mid;
	min = 0; max = ptr->nkey;
	if (min == max || ptr->data[0].idx > idx)
		return 0;
	while (true){
		if (min + 1 == max)
			return min + 1;
		mid = (min + max) / 2;
		if (ptr->data[mid].idx > idx)
			max = mid;
		else
			min = mid;
	}
}

block_sector_t set_parent(block_sector_t sector, block_sector_t parent){
	struct b_tree_node* ptr = (struct b_tree_node*)filesys_cache_block_get_write(sector);
	block_sector_t ret = ptr->parent;
	ptr->parent = parent;
	filesys_cache_block_release(ptr);
	return ret;
}

block_sector_t move_child(struct b_tree_node* src, 
						uint8_t srcoff, 
						struct b_tree_node* dst, 
						block_sector_t dst_sector, 
						uint8_t dstoff, 
						uint8_t size){
	block_sector_t ret = NULL_SECTOR;
	uint8_t i;
	struct b_tree_leaf *lsrc = (struct b_tree_leaf*)src;
	struct b_tree_leaf *ldst = (struct b_tree_leaf*)dst;
	ASSERT(src->height == dst->height);
	if (src->height == 0){
		memmove(ldst->data + dstoff, lsrc->data + srcoff, size * sizeof(struct b_tree_record));
		memmove(ldst->size + dstoff, lsrc->size + srcoff, size * sizeof(size_t));
	}else{
		for(i = 0 ; i < size; i++)
			ret = set_parent(src->data[i].sector, dst_sector);
		memmove(ldst->data + dstoff, lsrc->data + srcoff, size * sizeof(struct b_tree_record));
	}	
	return ret;
}

void b_tree_update_first(struct b_tree_node* ptr, sector_idx new_off){
	sector_idx old_off = ptr->data[0].idx;
	block_sector_t sector;

	ptr->data[0].idx = new_off;
	sector = ptr->parent;
	while (sector != NULL_SECTOR){
		uint8_t loc;
		ptr = (struct b_tree_node*) filesys_cache_block_get_write(sector);
		loc = b_tree_search_node(ptr, old_off);
		ASSERT(loc > 0 && ptr->data[loc-1].idx == old_off);
		ptr->data[loc-1].idx = new_off;
		sector = ptr->parent;
		filesys_cache_block_release(ptr);
		if (loc-1 != 0) break;
	}
}

void b_tree_node_add(struct b_tree_node* ptr, uint8_t loc, sector_idx idx, block_sector_t sector){
	ASSERT(ptr->nkey < TREE_ORDER);
	memmove(ptr->data + loc + 1, ptr->data + loc, (ptr->nkey - loc) * sizeof(struct b_tree_record));
	ASSERT(loc != 0);
	ptr->data[loc].idx = idx;
	ptr->data[loc].sector = sector;
	ptr->nkey++;
}

void b_tree_leaf_add(struct b_tree_leaf *ptr, uint8_t loc, sector_idx idx, block_sector_t sector, size_t size){
	ASSERT(ptr->nkey < LEAF_LEN)
	b_tree_node_add((struct b_tree_node*)ptr, loc, idx, sector);
	memmove(ptr->size + loc + 1, ptr->size + loc, (ptr->nkey - loc - 1)*sizeof(size_t));
	ptr->size[loc] = size;
}

bool b_tree_split_root(struct b_tree_node *ptr, uint8_t size, block_sector_t root_sector, uint8_t *sep){
	block_sector_t a,b;
	struct b_tree_node *aptr, *bptr;

	ASSERT(ptr->parent == NULL_SECTOR);
	*sep = size / 2;
	if (!free_map_allocate(1,&a))
		return false;
	if (!free_map_allocate(1,&b)){
		free_map_release(a,1);
		return false;
	}
	aptr = (struct b_tree_node*)filesys_cache_block_get_write(a);
	bptr = (struct b_tree_node*)filesys_cache_block_get_write(b);

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
	ptr->data[0].idx = aptr->data[0].idx;
	ptr->data[0].sector = a;
	ptr->data[1].idx = bptr->data[0].idx;
	ptr->data[1].sector = b;

	filesys_cache_block_release(bptr);
	filesys_cache_block_release(aptr);
	return true;
}

bool b_tree_split_next(struct b_tree_node* ptr, uint8_t size, uint8_t *sep, block_sector_t root){
	struct b_tree_node *next, *par;
	uint8_t loc;
	block_sector_t next_sector, ptr_sector;

	ASSERT(ptr->next == NULL_SECTOR);
	*sep = size / 2;
	if (!free_map_allocate(1, &next_sector))
		return false;
	next = (struct b_tree_node*)filesys_cache_block_get_write(next_sector);
	next->height = ptr->height;
	next->nkey = size - *sep;
	next->next = NULL_SECTOR;
	next->parent = ptr->parent;

	par = (struct b_tree_node*)filesys_cache_block_get_write(ptr->parent);
	loc = par->nkey;
	ptr_sector = move_child(ptr, *sep, next, next_sector, 0, next->nkey);
	if (!b_tree_insert_node(&par, &loc, next->data[0].idx, next_sector, root)){
		if (ptr_sector != NULL_SECTOR)
			move_child(next, 0, ptr, ptr_sector, *sep, next->nkey);
		filesys_cache_block_release(par);
		filesys_cache_block_release(next);
		free_map_release(next_sector, 1);
		return false;
	}

	ptr->nkey = *sep;
	ptr->next = next_sector;
	filesys_cache_block_release(par);
	filesys_cache_block_release(next);
	return true;
}

bool b_tree_split_node(struct b_tree_node* aptr, 
					struct b_tree_node* bptr, 
					uint8_t size, 
					uint8_t *sep1, 
					block_sector_t root){
	struct b_tree_node *cptr, *par;
	block_sector_t b, c;
	uint8_t loc;
	uint8_t sep2;

	ASSERT(aptr->nkey == size && bptr->nkey == size);
	*sep1 = size*2/3;
	sep2 = size*4/3;
	if (!free_map_allocate(1,&c))
		return false;
	cptr = (struct b_tree_node*)filesys_cache_block_get_write(c);
	cptr->height = aptr->height;
	cptr->nkey = 2*size-sep2;
	cptr->next = bptr->next;
	cptr->parent = bptr->parent;

	b = move_child(bptr, sep2-size, cptr, c, 0, cptr->nkey);
	par = (struct b_tree_node*)filesys_cache_block_get_write(cptr->parent);
	loc = b_tree_search_node(par, cptr->data[0].idx);
	if (!b_tree_insert_node(&par, &loc, cptr->data[0].idx, c, root)){
		if (b != NULL_SECTOR)
			move_child(cptr, 0, bptr, b, sep2-size, cptr->nkey);
		filesys_cache_block_release(par);
		filesys_cache_block_release(cptr);
		free_map_release(c, 1);
		return false;
	}
	move_child(bptr, 0, bptr, b, size-*sep1, sep2-size);
	b_tree_update_first(bptr, aptr->data[*sep1].idx);
	move_child(aptr, *sep1, bptr, b, 0, size-*sep1);
	aptr->nkey = *sep1;
	bptr->nkey = sep2 - *sep1;
	bptr->next = c;
	filesys_cache_block_release(par);
	filesys_cache_block_release(cptr);
	return true;
}

bool b_tree_insert_leaf(struct b_tree_leaf** ptr, 
						uint8_t *loc,
						sector_idx idx,
						block_sector_t sector,
						size_t size,
						block_sector_t root){
	struct b_tree_leaf *next = NULL;
	block_sector_t next_sector = (*ptr)->next;

	// boundary check
	ASSERT(*loc == (*ptr)->nkey || idx+size <= (*ptr)->data[*loc].idx);
	ASSERT(*loc == 0 || (*ptr)->data[*loc-1].idx + (*ptr)->size[*loc-1] <= idx);

	if ((*ptr)->nkey < LEAF_LEN){
		// There are free space there!
		ASSERT(*loc != 0);
		b_tree_leaf_add(*ptr, *loc, idx, sector, size);
		return true;
	}else if ((*ptr)->parent == NULL_SECTOR){
		// if ptr is root node
		uint8_t sep;
		block_sector_t child_sector;
		if (!b_tree_split_root((struct b_tree_node*)(*ptr), LEAF_LEN, root, &sep))
			return false;
		if (*loc >= sep){
			child_sector = (*ptr)->data[1].sector;
			*loc -= sep;
		}else
			child_sector = (*ptr)->data[0].sector;
		filesys_cache_block_release(*ptr);
		*ptr = (struct b_tree_leaf*)filesys_cache_block_get_write(child_sector);
		b_tree_leaf_add(*ptr, *loc, idx, sector, size);
		return true;
	}else if (next_sector == NULL_SECTOR){
		// if next node is not exist.
		uint8_t sep;
		if (!b_tree_split_next((struct b_tree_node*)(*ptr), LEAF_LEN, &sep, root))
			return false;
		if (*loc >= sep){
			next_sector = (*ptr)->next;
			filesys_cache_block_release(*ptr);
			*ptr = (struct b_tree_leaf*)filesys_cache_block_get_write(next_sector);
			*loc -= sep;
		}
		b_tree_leaf_add(*ptr, *loc, idx, sector, size);
		return true;
	}else if ((next=(struct b_tree_leaf*)filesys_cache_block_get_write(next_sector))->nkey < LEAF_LEN){
		// if exist next node and it has free space
		if (*loc == LEAF_LEN){
			*loc = 0;
			filesys_cache_block_release(*ptr);
			*ptr = next;
		}else{
			struct b_tree_record *swap = (*ptr)->data + LEAF_LAST_ELEM;
			b_tree_leaf_add(next, 0, swap->idx, swap->sector, (*ptr)->size[LEAF_LAST_ELEM]);
			filesys_cache_block_release(next);
			(*ptr)->nkey--;
		}
		b_tree_leaf_add(*ptr, *loc, idx, sector, size);
		return true;		
	}else{
		// both this and next node are full.
		uint8_t sep;
		if (!b_tree_split_node((struct b_tree_node*)(*ptr), (struct b_tree_node*)next, LEAF_LEN, &sep, root)){
			filesys_cache_block_release(next);
			return false;
		}
		if (*loc >= sep){
			*loc-=sep;
			filesys_cache_block_release(*ptr);
			*ptr = next;
		}else
			filesys_cache_block_release(next);
		b_tree_leaf_add(*ptr, *loc, idx, sector, size);
		return true;
	}
}

bool b_tree_insert_node(struct b_tree_node **ptr, uint8_t *loc, 
						sector_idx idx, 
						block_sector_t sector, 
						block_sector_t root){
	struct b_tree_node * next = NULL;
	block_sector_t next_sector = (*ptr)->next;

	ASSERT(*loc == 0 || (*ptr)->data[*loc-1].idx != idx);

	if ((*ptr)->nkey < TREE_ORDER){
		// There are free space there!
		ASSERT(*loc != 0);
		b_tree_node_add(*ptr, *loc, idx, sector);
		return true;
	}else if ((*ptr)->parent == NULL_SECTOR){
		// if *ptr is root node
		uint8_t sep;
		block_sector_t child_sector;
		if (!b_tree_split_root(*ptr, TREE_ORDER, root, &sep))
			return false;
		if (*loc >= sep){
			child_sector = (*ptr)->data[1].sector;
			*loc -= sep;
		}else
			child_sector = (*ptr)->data[0].sector;
		filesys_cache_block_release(*ptr);
		*ptr = (struct b_tree_node*)filesys_cache_block_get_write(child_sector);
		set_parent(sector, child_sector);
		b_tree_node_add(*ptr, *loc, idx, sector);
		return true;
	}else if(next_sector == NULL_SECTOR){
		// if next node is not exist.
		uint8_t sep;
		if (!b_tree_split_next(*ptr, TREE_ORDER, &sep, root))
			return false;
		if (*loc >= sep){
			next_sector = (*ptr)->next;
			filesys_cache_block_release(*ptr);
			*ptr = (struct b_tree_node*)filesys_cache_block_get_write(next_sector);
			*loc-=sep;
			set_parent(sector, next_sector);
		}
		b_tree_node_add(*ptr, *loc, idx, sector);
		return true;
	}else if ((next=(struct b_tree_node*) filesys_cache_block_get_write(next_sector))->nkey < TREE_ORDER){
		// if exist next node and it has free space
		if (*loc == TREE_ORDER){
			*loc = 0;
			filesys_cache_block_release(*ptr);
			*ptr = next;
			set_parent(sector, next_sector);
		}else{
			struct b_tree_record *swap = (*ptr)->data + LAST_ELEM;
			set_parent(swap->sector, next_sector);
			b_tree_node_add(next, 0, swap->idx, swap->sector);
			filesys_cache_block_release(next);
			(*ptr)->nkey--;
		}
		b_tree_node_add(*ptr, *loc, idx, sector);
		return true;
	}else{
		// both this and next node is full.
		uint8_t sep;
		if (!b_tree_split_node(*ptr, next, TREE_ORDER, &sep, root)){
			filesys_cache_block_release(next);
			return false;
		}
		if (*loc >= sep){
			*loc -= sep;
			filesys_cache_block_release(*ptr);
			*ptr = next;
		}else
			filesys_cache_block_release(next);
		b_tree_node_add(*ptr, *loc, idx ,sector);
		return true;
	}
}

void b_tree_remove(block_sector_t sector){
	struct b_tree_node* ptr = (struct b_tree_node*)filesys_cache_block_get_read_only(sector);
	int i;
	if (ptr->height)
		for(i = 0; i < ptr->nkey; i++)
			b_tree_remove(ptr->data[i].sector);
	else{
		struct b_tree_leaf* lptr = (struct b_tree_leaf*)ptr; 
		for(i = 0; i < lptr->nkey; i++)
			free_map_release(lptr->data[i].sector, lptr->size[i]);
	};
	filesys_cache_block_release(ptr);
	free_map_release(sector, 1);
}

struct inode
{
	struct list_elem elem;			/* Element in inode list. */
	block_sector_t sector;			/* Sector number of disk location. */
	int open_cnt;					/* Number of openers. */
	bool removed;					/* True if deleted, false otherwise. */
	int deny_write_cnt;				/* 0: writes ok, >0: deny writes. */
	uint8_t flag;
	struct lock lock;				
};

static struct list open_inodes;

void inode_init(void){
	list_init(&open_inodes);
}

bool inode_create(block_sector_t sector, off_t length, uint8_t flag){
	struct b_tree_leaf* ptr = (struct b_tree_leaf*)filesys_cache_block_get_write(sector);
	block_sector_t child_sector;
	size_t size = DIV_ROUND_UP(length, BLOCK_SECTOR_SIZE);
	ASSERT(sizeof(struct b_tree_node) == BLOCK_SECTOR_SIZE);
	ASSERT(sizeof(struct b_tree_leaf) == BLOCK_SECTOR_SIZE);
	ASSERT (length >= 0);
	ASSERT (sizeof(struct b_tree_node) == BLOCK_SECTOR_SIZE);
	child_sector = malloc_sector(&size);
	ptr->nkey = 1;
	ptr->height = 0;
	ptr->next = ptr->parent = NULL_SECTOR;
	ptr->length = (size_t)length;
	ptr->data[0].idx = 0;
	ptr->data[0].sector = child_sector;
	ptr->size[0] = size;
	ptr->flag = flag;
	filesys_cache_block_release(ptr);
	return true;
}

struct inode* inode_open(block_sector_t sector){
	struct list_elem *e;
	struct inode *inode;
	struct b_tree_node* ptr;

	for (e = list_begin(&open_inodes); e != list_end(&open_inodes); e = list_next(e)){
		inode = list_entry(e, struct inode, elem);
		if (inode->sector == sector){
			inode_reopen(inode);
			return inode;
		}
	}

	inode = malloc (sizeof(struct inode));
	if (inode == NULL)
		return NULL;

	list_push_front(&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;

	ptr = (struct b_tree_node*)filesys_cache_block_get_read_only(sector);
	inode->flag = ptr->flag;
	filesys_cache_block_release(ptr);

	lock_init(&inode->lock);
	return inode;
}

struct inode* inode_reopen(struct inode* inode){
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

block_sector_t inode_get_inumber(const struct inode* inode){
	return inode->sector;
}

void inode_close(struct inode *inode){
	if(inode==NULL)
		return;

	if (--inode->open_cnt == 0){
		list_remove(&inode->elem);
		if (inode->removed){
			b_tree_remove(inode->sector);
		}
		free(inode);
	}
}

void inode_remove(struct inode *inode){
	ASSERT(inode != NULL);
	inode->removed = true;
}

block_sector_t malloc_sector(size_t *size){
	size_t i;
	block_sector_t sector;
	if (*size == 0)
		return NULL_SECTOR;
	while(!free_map_allocate(*size, &sector)){
			if (*size == 1){
				*size = 0;
				return NULL_SECTOR;
			}
			*size /= 2;
	}
	for (i = 0; i < *size; i++){
		void* buf = filesys_cache_block_get_write(sector + i);
		memset(buf,0,BLOCK_SECTOR_SIZE);
		filesys_cache_block_release(buf);
	}
	return sector;
}

struct b_tree_iter{
	struct b_tree_leaf * ptr;
	uint8_t loc;
	sector_idx begin, end, root;
};

bool b_tree_iter_begin(struct b_tree_iter* iter, block_sector_t root, sector_idx begin, sector_idx end){
	uint8_t loc;
	struct b_tree_node* ptr;
	struct b_tree_leaf* lptr;
	sector_idx bound = 0xffffffff;
	block_sector_t child_sector;
	iter->begin = begin;
	iter->end = end;
	iter->root = root;

	if (begin >= end)
		return true;
	child_sector = root;
	while(true){
		ptr = (struct b_tree_node*) filesys_cache_block_get_read_only(child_sector);
		loc = b_tree_search_node(ptr, begin);
		ASSERT(loc != 0);
		if (loc < ptr->nkey){
			ASSERT(ptr->data[loc].idx <= bound);
			bound = ptr->data[loc].idx;
		}
		if (ptr->height == 0) break;
		child_sector = ptr->data[loc-1].sector;
		filesys_cache_block_release(ptr);
	
	}

	lptr = (struct b_tree_leaf*)ptr;
	if (lptr->data[loc-1].idx + lptr->size[loc-1] <= begin){
		size_t size;
		block_sector_t sector;
		ASSERT(bound > begin);
		size = min(bound, end) - begin;
		sector = malloc_sector(&size);
		if (sector == NULL_SECTOR){
			filesys_cache_block_release(ptr);
			return false;
		}
		set_dirty_flag(ptr);
		if (!b_tree_insert_leaf(&lptr, &loc, begin, sector, size, root)){
			filesys_cache_block_release(ptr);
			return false;
		}
	}else
		loc--;
	iter->ptr = lptr;
	iter->loc = loc;
	return true;
}


bool b_tree_iter_next(struct b_tree_iter* iter){
	size_t size;
	sector_idx next_idx;
	block_sector_t sector;

	ASSERT(iter->begin < iter->end);
	iter->begin = iter->ptr->data[iter->loc].idx + iter->ptr->size[iter->loc];
	iter->loc++;
	if (iter->begin >= iter->end){
		filesys_cache_block_release(iter->ptr);
		iter->ptr = NULL;
		return true;
	}
	if (iter->loc < iter->ptr->nkey){
		next_idx = iter->ptr->data[iter->loc].idx;
		ASSERT(iter->begin <= next_idx);
		if (iter->begin == next_idx)
			return true;
	}else if (iter->ptr->next != NULL_SECTOR){
		struct b_tree_leaf* next = (struct b_tree_leaf*)filesys_cache_block_get_read_only(iter->ptr->next);
		ASSERT(next->nkey>0);
		next_idx = next->data[0].idx;
		if (next_idx == iter->begin){
			filesys_cache_block_release(iter->ptr);
			iter->ptr = next;
			iter->loc = 0;
			return true;
		}else
			filesys_cache_block_release(next);
	}else
		next_idx = 0xffffffff;

	size = min(next_idx, iter->end) - iter->begin;
	sector = malloc_sector(&size);
	if (sector == NULL_SECTOR){
		filesys_cache_block_release(iter->ptr);
		return false;
	}
	set_dirty_flag(iter->ptr);
	if (!b_tree_insert_leaf(&(iter->ptr), &(iter->loc), iter->begin, sector, size, iter->root)){
		filesys_cache_block_release(iter->ptr);
		return false;
	}
	return true;
}

off_t inode_read_at(struct inode *inode, void *buffer, off_t size, off_t offset){
	sector_idx begin, end;
	struct b_tree_iter iter;
	off_t new_off, len;
	off_t byte_read = 0;
	void *_buf;

	if(size<=0)
		return 0;
	len = inode_length(inode);
	if (offset >= len)
		return 0;
	if (offset + size > len)
		size = len - offset;
	begin = offset / BLOCK_SECTOR_SIZE;
	end = DIV_ROUND_UP(offset + size, BLOCK_SECTOR_SIZE);

	if (!b_tree_iter_begin(&iter, inode->sector, begin, end))
		return byte_read;\
	while(size>0){
		size_t s;
		new_off = ROUND_UP(offset, BLOCK_SECTOR_SIZE);
		if (offset == new_off)
			new_off += BLOCK_SECTOR_SIZE;
		if (new_off - offset > size)
			new_off = offset + size;
		s = new_off - offset;
		begin = offset/ BLOCK_SECTOR_SIZE;
		if (begin == iter.ptr->data[iter.loc].idx + iter.ptr->size[iter.loc]){
			if (!b_tree_iter_next(&iter))
				return byte_read;
		}
		_buf = filesys_cache_block_get_read_only(iter.ptr->data[iter.loc].sector + begin - iter.ptr->data[iter.loc].idx);
		memcpy(buffer, _buf + offset - begin*BLOCK_SECTOR_SIZE, s);
		filesys_cache_block_release(_buf);
		buffer += s;
		size -= s;
		byte_read += s;
		offset = new_off;
	}
	b_tree_iter_next(&iter);
	ASSERT(iter.ptr == NULL);
	return byte_read;
}

off_t inode_write_at(struct inode *inode, const void *buffer, off_t size, off_t offset){
	sector_idx begin, end;
	struct b_tree_iter iter;
	off_t new_off;
	off_t byte_read = 0;
	void *_buf;

	if (inode->deny_write_cnt)
		return 0;
	struct b_tree_node* root = (struct b_tree_node*)filesys_cache_block_get_read_only(inode->sector);
	if (size <= 0)
		return 0;
	ASSERT(offset >= 0);
	if (root->length < (size_t)(offset + size)){
		set_dirty_flag(root);
		root->length = offset + size;
	}
	filesys_cache_block_release(root);
	
	begin = offset / BLOCK_SECTOR_SIZE;
	end = DIV_ROUND_UP(offset + size, BLOCK_SECTOR_SIZE);

	if (!b_tree_iter_begin(&iter, inode->sector, begin, end))
		return byte_read;\
	while(size>0){
		size_t s;
		new_off = ROUND_UP(offset, BLOCK_SECTOR_SIZE);
		if (offset == new_off)
			new_off += BLOCK_SECTOR_SIZE;
		if (new_off - offset > size)
			new_off = offset + size;
		s = new_off - offset;
		begin = offset/ BLOCK_SECTOR_SIZE;
		if (begin == iter.ptr->data[iter.loc].idx + iter.ptr->size[iter.loc]){
			if (!b_tree_iter_next(&iter))
				return byte_read;
		}
		_buf = filesys_cache_block_get_write(iter.ptr->data[iter.loc].sector + begin - iter.ptr->data[iter.loc].idx);
		memcpy(_buf + offset - begin*BLOCK_SECTOR_SIZE, buffer, s);
		filesys_cache_block_release(_buf);
		buffer += s;
		size -= s;
		byte_read += s;
		offset = new_off;
	}
	b_tree_iter_next(&iter);
	ASSERT(iter.ptr == NULL);
	return byte_read;
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
	struct b_tree_node* ptr = (struct b_tree_node*)filesys_cache_block_get_read_only(inode->sector);
	size_t size = ptr->length;
	filesys_cache_block_release(ptr);
	return size;
}

uint8_t inode_get_flag(struct inode* inode){
	return inode->flag;
}