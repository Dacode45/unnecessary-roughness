/*
 * mm.c - The fastest, least memory-efficient malloc package.
 *
 * We use a simple implicit list solution atm.
 * Propably will transform to seglist.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#include <assert.h>
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "unnecessary-roughness",
    /* First member's full name */
    "David Ayeke",
    /* First member's WUSTL key */
    "ayekedavidr",
    /* Second member's full name (leave blank if none) */
    "Ben Stolovitz",
    /* Second member's WUSTL key (leave blank if none) */
    "bstolovitz"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/**
 * Block macros
 *
 * void* previous
 * size_t size (union with 1 bit active)
 * (allocated data)
 *
 * My guess is we will eventually reorganize to:
 * void* previous
 * size_t size | free bit
 * (allocated data) | (void * next_of_same_size; void* prev_of_same_size)
 *
 * ie seglists. Chunk to powers of 2 seems reasonable.
 */
typedef void * block_node;
#define MAX(a, b) ((a >= b) ? a : b)
#define BLOCK_HEADER_SIZE ALIGN((sizeof(block_node) + sizeof(size_t)))
#define MIN_DATA_SIZE ALIGN(2*sizeof(block_node))

#define GET_BLOCK_SIZE(size) (ALIGN(MAX(size, MIN_DATA_SIZE) + BLOCK_HEADER_SIZE))
#define GET_HEADER(dataptr) ((block_node)((char*)dataptr - BLOCK_HEADER_SIZE))
#define GET_DATA(blockptr) ((void*)((char*)blockptr + BLOCK_HEADER_SIZE))

#define GET_PREVIOUS_BLOCK(currentBlock) *(block_node*)currentBlock
#define SET_PREVIOUS_BLOCK(blockPointer, previous) (GET_PREVIOUS_BLOCK(blockPointer) = (block_node)previous)
#define GET_NEXT_BLOCK(currentBlock) (block_node)((char*)currentBlock + BLOCK_HEADER_SIZE + GET_SIZE(currentBlock))

#define GET_NEXT_FREE_BLOCK(currentBlock) (*(block_node*)((char*)currentBlock + BLOCK_HEADER_SIZE))
#define SET_NEXT_FREE_BLOCK(currentBlock, next) (GET_NEXT_FREE_BLOCK(currentBlock) = (block_node)next)
#define GET_PREVIOUS_FREE_BLOCK(currentBlock) (*(block_node*)((char*)currentBlock + BLOCK_HEADER_SIZE + sizeof(block_node))) // stub
#define SET_PREVIOUS_FREE_BLOCK(currentBlock, prev) (GET_PREVIOUS_FREE_BLOCK(currentBlock) = (block_node)prev)

#define FREE_MASK (1<<(sizeof(size_t)*8 - 1))
#define GET_MASKED_SIZE_POINTER(blockPointer) ((size_t*)((char*)blockPointer + sizeof(block_node)))
#define GET_MASKED_SIZE(blockPointer) (*GET_MASKED_SIZE_POINTER(blockPointer))
#define IS_FREE(blockPointer) (GET_MASKED_SIZE(blockPointer) & FREE_MASK)
#define SET_FREE(blockPointer, free) (GET_MASKED_SIZE(blockPointer) = !!(free) << (sizeof(size_t)*8 - 1) | (GET_MASKED_SIZE(blockPointer) << 1 >> 1))
#define GET_SIZE(blockPointer) (GET_MASKED_SIZE(blockPointer) & ~FREE_MASK)
#define SET_SIZE(blockPointer, size) ((GET_MASKED_SIZE(blockPointer)) = size | IS_FREE(blockPointer))

/**
 * Bookkeeping
 */
block_node BASE = NULL;
block_node END = NULL;

//Let this be a power of two. aka MIN_BIN_SIZE = 2 means min bin size 2^2 or 4
//Bins are [2^(i-1),i)
#define MIN_BIN_SIZE 2
#define FREE_LIST_COUNT 5
block_node FREE_LIST[FREE_LIST_COUNT];
//WE NEED to assign ends of list for edgecase

//TODO integrate
static inline void push_free(block_node block, int bin){

  assert(bin >= 0 && bin < FREE_LIST_COUNT);
  GET_NEXT_FREE_BLOCK(block);
  assert(block != NULL);

  SET_NEXT_FREE_BLOCK(block, FREE_LIST[bin]);
  SET_PREVIOUS_FREE_BLOCK(block, NULL);
  FREE_LIST[bin] = block;
}
//Remove block from middle of list, restructure the list.
//NOTE must ensure that block isn't accidently added back
//returns block
static inline void remove_free(block_node block){
  if(!IS_FREE(block)){
    return;
  }
  block_node prev_block = GET_PREVIOUS_FREE_BLOCK(block);
  block_node next_block = GET_NEXT_FREE_BLOCK(block);
  if(next_block != NULL){
    if(prev_block != NULL){
      SET_NEXT_FREE_BLOCK(prev_block, next_block);
      SET_PREVIOUS_FREE_BLOCK(next_block, prev_block);
      return;
    }
    SET_PREVIOUS_FREE_BLOCK(next_block, NULL);
    return;
  }
  if(prev_block != NULL){
    SET_NEXT_FREE_BLOCK(prev_block, NULL);
  }else{
    FREE_LIST[get_bin(GET_SIZE(block))] = NULL;
  }
}

static inline block_node pop_free(int bin){
  assert(bin >= 0 && bin < FREE_LIST_COUNT);
  block_node block= FREE_LIST[bin];
  FREE_LIST[bin] = GET_NEXT_FREE_BLOCK(block);
  if(FREE_LIST[bin]){
    SET_PREVIOUS_FREE_BLOCK(FREE_LIST[bin], NULL);
  }
  return block;

}

int get_bin(size_t size){
  int index;
  int bin_size = 1 << MIN_BIN_SIZE;
  for(index = 0; index < FREE_LIST_COUNT-1; index++ ){
    if(size <= bin_size){
      return index;
    }
    bin_size << 1;
  }
  return FREE_LIST_COUNT-1;
}

/*
 * returns non-zero if macros function properly
 */
int macro_checker()
{
  /**
   * BLOCK_HEADER_SIZE
   */
  // should be 8 bytes
  // b/c sizeof(void*) = 4, sizeof(size_t) = 4
  // 4 + 4 = 8
  assert(sizeof(void*) == 4);
  assert(sizeof(size_t) == 4);
  assert(BLOCK_HEADER_SIZE == 8);

  /**
   * Header & Data getters
   */
  // should be inverses
   const size_t testNodeSize = 8;
  block_node testNode = malloc(GET_BLOCK_SIZE(testNodeSize));
  assert(GET_HEADER(GET_DATA(testNode)) == testNode);
  // should move the right amount (ie BLOCK_HEADER_SIZE)
  // because: o o o o|o o o o w/ header_size = 4
  //         ^
  // should:  o o o o|o o o o
  //                 ^
  assert(GET_DATA(testNode) == testNode + BLOCK_HEADER_SIZE);

  /**
   * Size and Free
   */
  // should accurately read a free set
  GET_MASKED_SIZE(testNode) = 1 << (sizeof(size_t)*8 - 1);
  assert(IS_FREE(testNode));
  assert(GET_SIZE(testNode) == 0);
  // should accurately read free set even if size is still set
  GET_MASKED_SIZE(testNode) = ~0;
  assert(IS_FREE(testNode));
  // should accurately read a non-free set
  GET_MASKED_SIZE(testNode) = GET_MASKED_SIZE(testNode) >> 1;
  assert(!IS_FREE(testNode));
  assert(GET_SIZE(testNode) == ~(1 << (sizeof(size_t)*8 - 1)));

  // should accurately write frees
  SET_FREE(testNode, 1);
  assert(IS_FREE(testNode));
  SET_FREE(testNode, 0);
  assert(!IS_FREE(testNode));
  // should accurately write frees even if not 1 or 0
  SET_FREE(testNode, 1505);
  assert(IS_FREE(testNode));

  // should accurately write sizes
  const size_t testSize = 5244;
  SET_SIZE(testNode, testSize);
  assert(GET_SIZE(testNode) == testSize);

  // should NOT overwrite size or free on writes to other
  SET_FREE(testNode, 0);
  const size_t anotherTestSize = 12;
  SET_SIZE(testNode, anotherTestSize);
  assert(!IS_FREE(testNode));
  SET_FREE(testNode, 1);
  assert(GET_SIZE(testNode) == anotherTestSize);
  const size_t yetAnotherTestSize = 8900444;
  SET_SIZE(testNode, yetAnotherTestSize);
  assert(IS_FREE(testNode));
  SET_FREE(testNode, 0);
  assert(GET_SIZE(testNode) == yetAnotherTestSize);

  /**
   * Block traversal
   */
  // set up some dummy values (previous ptr and size)
  SET_PREVIOUS_BLOCK(testNode, 0xbee71e5);
  SET_SIZE(testNode, ALIGN(testNodeSize));
  SET_FREE(testNode, 1);
  SET_NEXT_FREE_BLOCK(testNode, 0xf00d);
  SET_PREVIOUS_FREE_BLOCK(testNode, 0xba51c);
  // should properly find the next and previous blocks
  assert(GET_NEXT_BLOCK(testNode) == (char*)testNode + BLOCK_HEADER_SIZE + ALIGN(testNodeSize));
  assert(GET_PREVIOUS_BLOCK(testNode) == (block_node)0xbee71e5);
  assert(GET_NEXT_FREE_BLOCK(testNode) == (block_node)0xf00d);
  assert(GET_PREVIOUS_FREE_BLOCK(testNode) == (block_node)0xba51c);

  printf("Macros checked successfully!\n");

  return 1;
}

/*
 * Just a simple checker function to verify that no blocks
 * are the same as a block_node check.
 *
 * Returns 1 on uniqueness.
 *
 */
int check_unique(block_node check)
{
  block_node current = END;
  while(current != NULL) {
    if (check == current){
      return 0;
    }
    current = GET_PREVIOUS_BLOCK(current);
  }
  return 1;
}

/*
 * returns non-zero if heap is sensible.
 */
int mm_check(void)
{
int has_failed = 0;
  printf("BASE: %p; END: %p\n", BASE, END);

  for(size_t i = 0; i < FREE_LIST_COUNT; ++i) {
    printf("FREE_LIST[%lu]: %p\n",
      i,
      FREE_LIST[i]);
  }

  block_node current = BASE;
  block_node last = NULL;
  while((current != NULL) && (last < END)) {
    printf("\tCHECKING: %p; SIZE: %#lx; FREE: %d; NEXT: %p; PREVIOUS: %p\n",
      current,
      GET_SIZE(current),
      !!IS_FREE(current),
      GET_NEXT_BLOCK(current),
      GET_PREVIOUS_BLOCK(current));

    if(IS_FREE(current)) {
      printf("\t\tNEXTFREE: %p; PREVFREE: %p\n",
        GET_NEXT_FREE_BLOCK(current),
        GET_PREVIOUS_FREE_BLOCK(current));
    }

    // Check alignment of previous pointers
    if(last != GET_PREVIOUS_BLOCK(current)) {
      printf("Continuity error! %p doesn't pt to prev block %p, instead %p.\n",
        current,
        last,
        GET_PREVIOUS_BLOCK(current));
      has_failed = 1;
    }

    // Check contiguity of free space (with previous)
    if(last && IS_FREE(last) & IS_FREE(current)) {
      printf("Free error! %p is not connected to its free predecessor. \n",
        current);
      has_failed = 1;
    }

    last = current;
    current = GET_NEXT_BLOCK(current);
  }

  for(size_t i = 0; i < FREE_LIST_COUNT; ++i) {
    current = FREE_LIST[i];
    last = NULL;
    while(current != NULL) {
      printf("\tFREE[%lu]CCK: %p; NEXT: %p; PREV: %p\n",
        i,
        current,
        GET_NEXT_FREE_BLOCK(current),
        GET_PREVIOUS_FREE_BLOCK(current));

      // Check for pointer alignment
      if(last != GET_PREVIOUS_FREE_BLOCK(current)) {
        printf("Continuity error! %p doesn't pt to prev block %p, instead %p.\n",
          current,
          last,
          GET_PREVIOUS_FREE_BLOCK(current));
        has_failed = 1;
      }

      sleep(1);

      last = current;
      current = GET_NEXT_FREE_BLOCK(current);
    }
  }

  if(!has_failed) {
    printf("Done mm_check!\n");
  } else {
    printf("!!!! Failed mm_check.\n");
    exit(1);
  }
  return !has_failed;
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  #ifdef DEBUG
  #ifndef NDEBUG
    if(!macro_checker()) {
      return 0;
    }
  #endif
  #endif

  BASE = NULL;
  END = NULL;

  for (size_t i = 0; i < FREE_LIST_COUNT; ++i) {
    FREE_LIST[i] = NULL;
  }

  #ifdef DEBUG
  #ifndef NDEBUG
    mm_check();
  #endif
  #endif

  return 0;
}

/*
 * find_free finds a free block for your allocations!
 * That's right! Pass it a size_t request_size and it will make
 * all your dreams come true, finding the first available block
 * that holds your data if one exists.
 *
 * Otherwise returns NULL. How sensible!
 *
 */
block_node find_free(size_t requestSize)
{
  block_node LAST_CHECK = END;
  requestSize = ALIGN(requestSize);

  // Traverse backwards
  // Choose first available.
  while(!IS_FREE(LAST_CHECK) || GET_SIZE(LAST_CHECK) < requestSize) {
    //printf("\tCHECKING FREE BLOCKS: %p; SIZE Difference: %d; FREE: %d;\n", LAST_CHECK, GET_SIZE(LAST_CHECK) - requestSize, !!IS_FREE(LAST_CHECK));
    if(LAST_CHECK == BASE){
      // printf("\t\tNo Free BLOCK, MARGIN: %lu\n", most_space - requestSize);
      return NULL;
    }

    LAST_CHECK = GET_PREVIOUS_BLOCK(LAST_CHECK);
  }
  // printf("\t\tBLOCK: %p, IS FREE: %d, MARGIN: %lu\n", LAST_CHECK, !!IS_FREE(LAST_CHECK), GET_SIZE(LAST_CHECK) - requestSize);

  return LAST_CHECK;
}

/*
 * Gotta get somewhere fast? And by that, I mean do you need space quickly?
 * OK! Just tell us where the last block_node is, and the size you need, and
 * we'll give you a new block. Fast.
 *
 * Returns a pointer to the block if successful, NULL otherwise.
 *
 */
block_node request_space(block_node last, size_t size)
{
  // printf("\tIN request_space LAST: %p\n", last );

  block_node block = mem_sbrk(0);
  void* request = mem_sbrk(GET_BLOCK_SIZE(size));

  if(request == (void*)-1) {
    return NULL;
  }

  assert(request != NULL);
  assert(check_unique(block));
  assert(check_unique(request));
  assert((unsigned long)request % ALIGNMENT == 0);

  if(last) {
    SET_PREVIOUS_BLOCK(block, last);
  }
  SET_SIZE(block, ALIGN(size));
  SET_FREE(block, 1);
  assert((int)block % ALIGNMENT == 0);
  // printf("END: %p, BLOCK: %p\n",END, block );
  END = block;
  // printf("END: %p\n", *(block_node*)END);
  // printf("\t\tBLOCK: %p, IS GRANTED: \n",block );

  return block;
}

/*
 * There comes a time in any block's life where it needs to be split.
 * This can be a confusing and challenging experience, no matter whether
 * you're a free block or an occupied block, big or small.
 *
 * Luckily, we do this for you. Splits a block in two, with a given splitSize
 * for the first one. Puts the rest in a new block. Please don't call this
 * on something too big.
 *
 * This function does NOT coallesce.
 *
 */
block_node split_block(block_node block, size_t splitSize)
{
  // printf("\tIN split_block\n" );
  splitSize = ALIGN(splitSize);
  assert(splitSize < GET_SIZE(block) + BLOCK_HEADER_SIZE);
  //don't split unless can fit
  if(GET_SIZE(block) - splitSize < GET_BLOCK_SIZE(0)){
    return;
  }
  int isEnd = (block == END);
  block_node nextBlock = NULL;
  if (!isEnd) {
    nextBlock = GET_NEXT_BLOCK(block);
  }

  size_t oldSize = GET_SIZE(block);
  SET_SIZE(block, splitSize);

  // Make a new block
  block_node splitBlock = GET_NEXT_BLOCK(block);
  SET_PREVIOUS_BLOCK(splitBlock, block);
  SET_FREE(splitBlock, 1);
  size_t splitBlockSize = oldSize-(splitSize + BLOCK_HEADER_SIZE);
  SET_SIZE(splitBlock, splitBlockSize);

  if(!isEnd){
    SET_PREVIOUS_BLOCK(nextBlock, splitBlock);
  } else {
    END = splitBlock;
  }

  //Implement free list stuff
  int bin = get_bin(splitBlockSize);
  push_free(splitBlock, bin);
  // printf("\t\tBLOCK: %p, IS SPLIT INTO BLOCKS %p AND %p\n",block, block, s_block);
  return block;
}

/*
 * mm_malloc - A naive implicit-list approach.
 */
void *mm_malloc(size_t size)
{
  // printf("IN mm_malloc, SIZE = %#lx\n", size );

  if (size <= 0) {
    return NULL;
  }

  block_node block = NULL;

  // If first call, don't bother checking for free block.
  if(BASE) {
    block = find_free(size);
  }

  // If no free block, request space.
  // Otherwise, split the block if necessary.
  if(!block) {
    block = request_space(END, size);
  } else {
    assert((int)block % ALIGNMENT == 0);

    // TODO this size check needs to be based on size of each FREE_LIST
    if(GET_SIZE(block) > GET_BLOCK_SIZE(size)) {
      // printf("\tsplitting FREE Block, MARGIN: %lu, running average: %f \n", GET_SIZE(block)- GET_BLOCK_SIZE(size), AVERAGE_REQUEST_SIZE);
      block = split_block(block, size);
    }
  }

  if(!block) {
    printf("ERROR! Could not allocate :(");
    return NULL;
  }
  printf("\tCHECKING: %p; SIZE: %#lx; FREE: %d; NEXT: %p; PREVIOUS: %p\n",
    block,
    GET_SIZE(block),
    !!IS_FREE(block),
    GET_NEXT_BLOCK(block),
    GET_PREVIOUS_BLOCK(block));

  remove_free(block);
  SET_FREE(block, 0);

  if(!BASE) {
    BASE = block;
  }

  // printf("\tBLOCK: %p, IS MALLOC\n",block);
  #ifdef DEBUG
  #ifndef NDEBUG
    mm_check();
  #endif
  #endif

  return GET_DATA(block);
}

/*
 * mm_free - simple free with dual-directional coalescing!
 */
void mm_free(void *ptr)
{
  // printf("IN mm_free\n" );

  // We can't free nothing!
  if(!ptr || ((unsigned long)ptr % ALIGNMENT != 0)){
    return;
  }

  block_node block = GET_HEADER(ptr);
  assert((unsigned long)block % 8 == 0);

  SET_FREE(block, 1);
  // printf("\tFreeing BLOCK: %p\n", block);

  block_node prev = NULL;
  if(block != BASE) {
    prev = GET_PREVIOUS_BLOCK(block);
  }

  block_node next = NULL;
  if(block != END) {
    next = GET_NEXT_BLOCK(block);
  }

  // Coalesce left
  if(prev && IS_FREE(prev)){
    assert(GET_NEXT_BLOCK(prev) == block);

    remove_free(prev);
    size_t prevSize = GET_SIZE(prev) + GET_SIZE(block) + BLOCK_HEADER_SIZE;
    SET_SIZE(prev, prevSize);

    if(next) {
      SET_PREVIOUS_BLOCK(next, prev);
    } else {
      END = prev;
    }
    // printf("\tBLOCK: %p, has been merged left into BLOCK: %p\n",block, prev );

    //implement free list stuff
    block = prev;
  }

  // Coalesce right
  if(next && IS_FREE(next)) {
    assert(GET_PREVIOUS_BLOCK(next) == block);

    size_t currentSize = GET_SIZE(block);
    SET_SIZE(block, currentSize + GET_SIZE(next) + BLOCK_HEADER_SIZE);

    if (next == END) {
      END = block;
    } else {
      SET_PREVIOUS_BLOCK(GET_NEXT_BLOCK(next), block);
    }
    // printf("\tBLOCK: %p, has been merged right into BLOCK: %p\n",block, next );
    //implement free list stuff
    remove_free(next);
  }
  //implement free list
  printf("\tCHECKING free: %p; SIZE: %#lx; FREE: %d; NEXT: %p; PREVIOUS: %p\n",
    block,
    GET_SIZE(block),
    !!IS_FREE(block),
    GET_NEXT_FREE_BLOCK(block),
    GET_PREVIOUS_FREE_BLOCK(block));

  push_free(block, get_bin(GET_SIZE(block)));
  printf("\tCHECKING free: %p; SIZE: %#lx; FREE: %d; NEXT: %p; PREVIOUS: %p\n",
    block,
    GET_SIZE(block),
    !!IS_FREE(block),
    GET_NEXT_FREE_BLOCK(block),
    GET_PREVIOUS_FREE_BLOCK(block));

  #ifdef DEBUG
  #ifndef NDEBUG
    mm_check();
  #endif
  #endif
}

/*
 * mm_realloc - Fairly dumb, tries to avoid reallocation by checking size
 * and trying to coalesce first.
 */
void *mm_realloc(void *ptr, size_t size)
{
  if(!ptr) {
    return mm_malloc(size);
  }

  block_node block = GET_HEADER(ptr);
  size_t currentSize = GET_SIZE(block);
  if(currentSize >= size) {
    return ptr;
  }

  // Coallesce if possible :)
  block_node next = NULL;
  if(block != END) {
    next = GET_NEXT_BLOCK(block);
  }

  if(next && IS_FREE(next) && currentSize + GET_SIZE(next) >= size) {
    assert(GET_PREVIOUS_BLOCK(next) == block);

    size_t currentSize = GET_SIZE(block);
    SET_SIZE(block, currentSize + GET_SIZE(next) + BLOCK_HEADER_SIZE);

    if (next == END) {
      END = block;
    } else {
      SET_PREVIOUS_BLOCK(GET_NEXT_BLOCK(next), block);
    }

    return ptr;
  }

  void* new_ptr;
  new_ptr = mm_malloc(size);
  if(!new_ptr) {
    return NULL;
  }
  memcpy(new_ptr, ptr, GET_SIZE(block)); // :(
  mm_free(ptr);
  return new_ptr;
}
