#include "inode_manager.h"

#define yes 1
#define no 0
#define debug no

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
	if (debug)
		printf("\tdebug: disk: disk blocks size:%ld\n", sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
	if (id < 0 || id > BLOCK_NUM) {
		printf("\terror: disk::read_block id %d out of range\n", id);
		return ;
	}
	if (buf == NULL) {
		printf("\terror: disk::read_block buf NULL\n");
		return;
	}
	if (debug)
		printf("\tdebug: disk::read_block %d\n", id);
	
	memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
	if (buf == NULL) {
		printf("\terror: disk::write_block buf NULL\n");
    	return;
	}
	if (id < 0 || id > BLOCK_NUM) {
		printf("\terror: disk::write_block id %d out of range\n", id);
	}
	if (debug){
		printf("\tdebug: disk::write_block %d with %s, len buf:%d\n",id,buf, (int)strlen(buf));//why struct inode strlen = 1
		if (strlen(buf) > 8){
			struct inode *ino = (struct inode*) buf;
			printf("\tdebug: disk::write inode size:%d, first block:%d\n", ino->size, ino->blocks[0]);
		}
	}
	// can not use strncpy, \0 may found
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
	
	/*
	const uint32_t data_begin = 1036;
	uint32_t map_size = using_blocks.size();
	for (uint32_t i=data_begin; i<data_begin + map_size; i++) {
		if (using_blocks.count(i) == 0) {
			using_blocks[i] = 1;
			return i;
		}
	}
	using_blocks[data_begin + map_size] = 1;
	return data_begin + map_size;
	*/
	
	blockid_t bitmap_id;//bitmap block begin from block 2 
	char tmp[BLOCK_SIZE];
	for (bitmap_id=2; bitmap_id<10; bitmap_id++) {// search 8 bitmap blocks, from 2 to 9
		read_block(bitmap_id, tmp);
		for (int i=0; i<BLOCK_SIZE; i++) {// search a block for free bit
			char byte = tmp[i];
			if (~byte) { // not byte true, means byte contains 0
				for (int pos=0; pos<8; pos++) {// search the bit 0
					if (~byte & (1<<pos)) {
						tmp[i] |= 1<<pos;
						write_block(bitmap_id, tmp);
						if (debug)
							printf("\tdebug: block_manager::alloc_block alloc block %d\n", (bitmap_id-2) * BPB + i * 8 + pos);
						return (bitmap_id-2) * BPB + i * 8 + pos;
					}
				}
			}
		}
	}

	printf("\terror: block_manager::alloc_block block use up\n");
	return -1;
	
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
	if (id < 0 || id > BLOCK_NUM) {
		printf("\terror: block_manager::free_block id %d out of range\n", id);
		return;
	}
	if (debug)
		printf("\tdebug: block_manager::free_block free block %d first\n", id);
	/*
  if (using_blocks.count(id) != 1)
		printf("\terror: free a non-existent block %d\n", id);
	if (using_blocks[id] == 0)
		printf("\terror: free a freed block %d\n", id);
	using_blocks[id] = 0;
	*/
	blockid_t bitmap_id = BBLOCK(id);	// the id of bitmap for the block to be freed
	int pos = id % BPB;					// the bit position in the bitmap block 
	char tmp[BLOCK_SIZE];
	read_block(bitmap_id, tmp);			// get corresponding block
	tmp[pos / 8] &= ~(1 << (pos % 8));	// pos / 8 is corresponding byte, and set the corresponding bit	
	write_block(bitmap_id, tmp);		// write back

	return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
	
	d = new disk();

	// format the disk
	sb.size = BLOCK_SIZE * BLOCK_NUM;
	sb.nblocks = BLOCK_NUM;
	sb.ninodes = INODE_NUM;
	// set 1 in bitmap for bitmap and inode table 
	blockid_t last_inode = IBLOCK(INODE_NUM, sb.nblocks); //1035
	char tmp[BLOCK_SIZE];
	memset(tmp, 0, BLOCK_SIZE);
	memset(tmp, ~0, last_inode / 8);
	char last_byte = 0;
	for (blockid_t i=0; i<last_inode%8; i++)
		last_byte |= 1 << (7 - i);
	tmp[last_inode / 8] = last_byte;

	d->write_block(BBLOCK(1), tmp);
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM) {
		printf("\terror: block_manager::read_block id %d out of range\n", id);
		return;
	}
	if (buf == NULL) {
		printf("\terror: block_manager::read_block buf NULL\n");
		return;
	}
	if (debug) 
		printf("\tdebug: bm::read_block %d\n", id);

	d->read_block(id, buf);

	if (debug)
		printf("\tdebug: bm::read_block %d get %s\n", id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
	if (id < 0 || id >= BLOCK_NUM) { 
		printf("\terror: block_manager::write_block id %d out of range\n", id);
		return;
	}
	if (buf == NULL) {
		printf("\terror: block_manager::write_block buf NULL\n"); 
		return;
	}
  if (debug)
		printf("\tdebug: bm::write_block %d with %s\n", id, buf);

	d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
	
	uint32_t inum = 1;//inum start from 1
	struct inode* ino;
	char tmp[BLOCK_SIZE];
	//if (debug)
	//	printf("\tdebug:inode size:%d,sb.ninodes%d\n", sizeof(struct inode), bm->sb.ninodes);
	// sizeof(struct inode) = 424, so why IPB == 1, mod?
	
	//find a suitable inum for allocate
	while (inum <= INODE_NUM) {
		bm->read_block(IBLOCK(inum, bm->sb.nblocks), tmp);
		ino = (struct inode*)tmp + inum%IPB; 
		if (ino == NULL || ino->type == 0)
			break;
		inum++;
	}
	if (inum == INODE_NUM) {
		printf("\terror: inode_manager::alloc_inode no inode can alloc\n");
		return -1;
	}
	// init inode
	ino->type = type;
	ino->size = 0;
	ino->atime = time(NULL);
	ino->mtime = ino->atime;
	ino->ctime = ino->ctime;
	bm->write_block(IBLOCK(inum, bm->sb.nblocks), tmp);
	
	if (debug)
		printf("\tdebug: im::alloc_inode type: %d, get inum %d\n", type, inum);
	return inum;
  
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
	
	if (inum < 0 || inum >= INODE_NUM) {
		printf("\terror: inode_manager::free_inode inum  %d out of range\n", inum);
		return;
	}
	struct inode* ino = get_inode(inum);
	if (ino == NULL) {
		printf("\terror: inode_manager::free_inode inode not exist\n");
		return;
	}
	if (ino->type == 0) {
		printf("\tim: free_inode already a freed one\n");
		return;
	}

	if (debug)
		printf("\tdebug: inode_manager::free_inode free inode %d\n", inum);

	ino->type = 0;
	memset(ino, 0, sizeof(struct inode));
	put_inode(inum, ino);

	return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {  // why inum in 0~1023 ?  with inum for root-dir = 1
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;
	
  printf("\tim: put_inode %d, first:%d, mtime:%d\n", inum, ino->blocks[0], ino->mtime);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
	
	if (debug)
		printf("\tdebug: im::put_inode inum:%d, first:%d, mtime:%d\n", inum, ino_disk->blocks[0], ino_disk->mtime);
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)<(b) ? (b) : (a))
/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
	
	// param check
	if (inum <= 0 || inum > INODE_NUM) {
		printf("\terror: inode_manager::read_file inum %d out of range\n",inum);
		return;
	}
	if (buf_out == NULL) {
		printf("\terror: inode_manager::read_file buf_out NULL\n");
		return;
	}
	if (size == NULL) {
		printf("\terror: inode_manager::read_file size NULL\n");
		return;
	}
	
	struct inode *ino = get_inode(inum);
	if (ino == NULL)
		printf("\terror: im::read_file, get inode %d failed\n", inum);

	// get size of inode blocks
	int block_num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;

	if (debug)
		printf("\tdebug: im::read_file, inum:%d, get type:%d, block_num: %d, first block:%d\n",\
					inum, ino->type, block_num, ino->blocks[0]);
	
	*buf_out = (char *)malloc(ino->size);
	char tmp[BLOCK_SIZE];
	// read 1st block to the second last
	for (int i=0; i<MIN(block_num, NDIRECT); i++) {
		bm->read_block(ino->blocks[i], tmp);
		if (debug)
			printf("\tdebug: im::read_file inode->blocks:%d,get %s\n", ino->blocks[i],tmp);
		if (i != block_num-1)
			memcpy(*buf_out + i*BLOCK_SIZE, tmp, BLOCK_SIZE);
		else
			memcpy(*buf_out + i*BLOCK_SIZE, tmp, ino->size - i*BLOCK_SIZE);
	}
	
	if (block_num > NDIRECT) {
		blockid_t indirectb[NINDIRECT];
		bm->read_block(ino->blocks[NDIRECT], (char *)indirectb);
		for (int i=NDIRECT; i<block_num; i++) {
			bm->read_block(indirectb[i - NDIRECT], tmp);
			if (i != block_num-1)
			memcpy(*buf_out + i*BLOCK_SIZE, tmp, BLOCK_SIZE);
		else
			memcpy(*buf_out + i*BLOCK_SIZE, tmp, ino->size - i*BLOCK_SIZE);
		}

	}

	if (debug)
		// what read all
		printf("\tdebug im::read_file what read %s\n", *buf_out);
	
	*size = ino->size;
	// update access time
	ino->atime = time(NULL);
	put_inode(inum, ino);
	return;
	
}


/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
	
	printf("\tim: write_file %d, size %d\n", inum, size);
	//param check
	if (inum <= 0 || inum > INODE_NUM || buf == NULL ) {
		printf("\terror: inode_manager::write_file inum %d out of range or buf NULL\n",inum);
		return;
	}

 	struct inode *ino = get_inode(inum);
	if (ino == NULL)
		printf("\terror: read_file, get inode %d failed\n", inum);
	
	if (debug)
		printf("\tdebug: im::write_file to inum %d, size = %d\n", inum, size);
		
	// get origin block num and needed block num now
	int oldNum = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	int newNum = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

	if (debug)
		printf("\tdebug: im::write_file old:%d, now:%d\n", oldNum, newNum);

	blockid_t indirectb[NINDIRECT];
	char data[BLOCK_SIZE];

    if (newNum <= oldNum) { // need free
        for (int i = 0; i < MIN(newNum, NDIRECT); i++) {
            if (i == newNum - 1) { // writing the last block
                bzero(data, BLOCK_SIZE);
                memcpy(data, buf + i * BLOCK_SIZE, size - i * BLOCK_SIZE);
                bm->write_block(ino->blocks[i], data);
            } else {
                bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
            }
        }
        if (newNum > NDIRECT) {// read indirect block
            bm->read_block(ino->blocks[NDIRECT], (char *)indirectb);
            for (int i = 0; i < newNum - NDIRECT; i++) {// write to old indirect
                if (i == newNum - NDIRECT - 1) { // writing the last block
                    bzero(data, BLOCK_SIZE);
                    memcpy(data, buf + (i + NDIRECT) * BLOCK_SIZE, size - (i + NDIRECT) * BLOCK_SIZE);
                    bm->write_block(indirectb[i], data);
                } else {
                    bm->write_block(indirectb[i],buf + (i + NDIRECT) * BLOCK_SIZE);
                }
            }
        }
        // free unused
        for (int i = newNum;i < MIN(oldNum, NDIRECT); i++)
            bm->free_block(ino->blocks[i]);
        if (oldNum > NDIRECT) {
            for (int i = MAX(newNum, NDIRECT);i < oldNum; i++) 
                bm->free_block(indirectb[i - NDIRECT]);
            // free indirect entry
            if (newNum <= NDIRECT) 
                bm->free_block(ino->blocks[NDIRECT]);
        }
    } else { // need alloc
        for (int i = 0; i < MIN(oldNum, NDIRECT); i++) {
            bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
        }
        for (int i = oldNum; i < MIN(newNum, NDIRECT); i++) {// for direct
            ino->blocks[i] = bm->alloc_block();
            if (i == newNum - 1) { // writing the last block
                bzero(data, BLOCK_SIZE);
                memcpy(data, buf + i * BLOCK_SIZE, size - i * BLOCK_SIZE);
                bm->write_block(ino->blocks[i], data);
            } else {
                bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
            }
        }
        if (newNum > NDIRECT) {// for indirect entry
            if (oldNum <= NDIRECT) {
                ino->blocks[NDIRECT] = bm->alloc_block();
            } else {
                bm->read_block(ino->blocks[NDIRECT], (char *)indirectb);
            }
            // write to old indirect
            for (int i = NDIRECT; i < MAX(oldNum, NDIRECT); i++) {
                bm->write_block(indirectb[i - NDIRECT], buf + i * BLOCK_SIZE);
            }
            // write to new-alloc indirect
            for (int i = MAX(oldNum, NDIRECT); i < newNum; i++) {
                indirectb[i - NDIRECT] = bm->alloc_block();
                if (i - NDIRECT == newNum - NDIRECT - 1) { //writing the last block
                    bzero(data, BLOCK_SIZE);
                    memcpy(data, buf + i * BLOCK_SIZE, size - i * BLOCK_SIZE);
                    bm->write_block(indirectb[i - NDIRECT], data);
                } else {
                    bm->write_block(indirectb[i - NDIRECT], buf + i * BLOCK_SIZE);
                }
            }
            if (newNum > NDIRECT) { // save new indirect entry
                bm->write_block(ino->blocks[NDIRECT], (char *)indirectb);
            }
        }
    }
    // update size and cmtime    
	ino->size = size;
	ino->mtime = time(NULL);
	ino->ctime = ino->mtime;
	put_inode(inum, ino);
	free(ino);
	return;

}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
	
	if (inum < 0 || inum > INODE_NUM) {
		printf("\terror: inode_manager::getattr inum %d out of range\n", inum);
		return;
	}
	struct inode* ino = get_inode(inum);
	if (ino == NULL) {
		printf("\terror: get inode %d in getattr not exist\n", inum);
		return;
	}
	a.type = ino->type;
	a.atime = ino->atime;
	a.mtime = ino->mtime;
	a.ctime = ino->ctime;
	a.size = ino->size; // important
	return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
	
	//param check
 	if (inum < 0 || inum >= INODE_NUM) {
		printf("\terror: inode_manager::remove_file inum %d out of range\n", inum);
		return;
	}
	
	struct inode *ino = get_inode(inum);
	if (ino == NULL) {
		printf("\terror: inode_manager::remove_file inode %d not exist\n", inum);
		return;
	}
	if (debug)
		printf("\tdebug: inode_manager::remove_file remove inum %d, inode size %d\n", inum, ino->size);

	int block_num = (ino->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	if (debug)
		printf("\tdebug: inode_manage::remove_file block num %d\n", block_num);

	for (int i=0; i< MIN(block_num, NDIRECT); i++) {
		bm->free_block(ino->blocks[i]);
	}
	if (block_num > NDIRECT) {
		blockid_t indirectbid = ino->blocks[NDIRECT]; 
		blockid_t indirectb[NINDIRECT];
		bm->read_block(indirectbid, (char *)indirectb);
		for (int i=0; i<block_num - NDIRECT; i++){
			bm->free_block(indirectb[i]);
		}
	}
	free_inode(inum);
	free(ino);
	
}
