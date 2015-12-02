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
#define BLOCK_HEADER_SIZE ALIGN((sizeof(block_node) + sizeof(size_t)))

#define GET_BLOCK_SIZE(size) (ALIGN((size) + BLOCK_HEADER_SIZE))
#define GET_HEADER(dataptr) ((block_node)((char*)dataptr - BLOCK_HEADER_SIZE))
#define GET_DATA(blockptr) ((void*)((char*)blockptr + BLOCK_HEADER_SIZE))

#define GET_PREVIOUS_BLOCK(currentBlock) *(block_node*)currentBlock
#define SET_PREVIOUS_BLOCK(blockPointer, previous) (GET_PREVIOUS_BLOCK(blockPointer) = (block_node)previous)
#define GET_NEXT_BLOCK(currentBlock) (block_node)((char*)currentBlock + BLOCK_HEADER_SIZE + GET_SIZE(currentBlock))

#define GET_NEXT_FREE_BLOCK(currentBlock, size) 0 // stub

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
block_node LAST_CHECK = NULL;
size_t LAST_CHECK_SIZE = 0;

// Flags
int FREE_CALLED = 0;

//Jeff McClintock running median eistimate
int AVERAGE_REQUEST_SIZE = 0.0f;
int NUM_REQUEST = 0;
void add_request(size){
  if(abs(size - AVERAGE_REQUEST_SIZE) > AVERAGE_REQUEST_SIZE){//Reset
    NUM_REQUEST = 0;
  }
  AVERAGE_REQUEST_SIZE += (size - AVERAGE_REQUEST_SIZE) * 1/(++NUM_REQUEST%15);
  // printf("\tIN add_request, size: %d, average: %d, number: %d\n", size, AVERAGE_REQUEST_SIZE, NUM_REQUEST );
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
  // *(block_node*)testNode = (block_node)0xbee71e5;
  SET_PREVIOUS_BLOCK(testNode, 0xbee71e5);
  SET_SIZE(testNode, ALIGN(testNodeSize));
  // should properly find the next and previous blocks
  assert(GET_NEXT_BLOCK(testNode) == (char*)testNode + BLOCK_HEADER_SIZE + ALIGN(testNodeSize));
  assert(GET_PREVIOUS_BLOCK(testNode) == (block_node)0xbee71e5);

  printf("Macros checked successfully!\n");

  return 1;
}

/*
 * returns non-zero if heap is sensible.
 */
int mm_check(void)
{
  int has_failed = 0;
  printf("BASE: %p; END: %p\n", BASE, END);

  block_node current = BASE;
  block_node last = NULL;
  while(current != NULL & last < END) {
    printf("\tCHECKING: %p; SIZE: %#lx; FREE: %d; NEXT: %p; PREVIOUS: %p\n", current, GET_SIZE(current), !!IS_FREE(current), GET_NEXT_BLOCK(current), GET_PREVIOUS_BLOCK(current));

    // Check contiguity of free space (with previous)
    if(last == NULL) {
    } else if(IS_FREE(last) & IS_FREE(current)) {
      printf("Free error! %p is not connected to its free predecessor. \n", current);
      has_failed = 1;
    }

    // Check alignment of previous pointers.
    if(last != GET_PREVIOUS_BLOCK(current)) {
      printf("Continuity error! %p doesn't point to previous block (%p), instead %p.\n", current, last, GET_PREVIOUS_BLOCK(current));
      has_failed = 1;
    }

    // TODO remove call from final code.
    // sleep(1);
    last = current;
    current = GET_NEXT_BLOCK(current);
  }

  if(!has_failed) {
    printf("Done mm_check!\n");
  } else {
    printf("!!!! Failed mm_check.\n");
  }
  return !has_failed;
}

/*
 * mm_init - initialize the malloc package.
 TODO check for multiple calls
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
  LAST_CHECK = NULL;
  LAST_CHECK_SIZE = 0;

  FREE_CALLED = 0;

  AVERAGE_REQUEST_SIZE = 0.0f;
  NUM_REQUEST = 0;

  #ifdef DEBUG
  #ifndef NDEBUG
    mm_check();
  #endif
  #endif

  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

//Go 1 past, and check the size of previous, best fit reduces fragmentation
//TODO add implment split
//ALWAYS SPLIT
block_node find_free(size_t request_size){
  // // printf("\tIN find_free\n" );
  request_size = ALIGN(request_size);
  if(LAST_CHECK == NULL || LAST_CHECK == BASE || FREE_CALLED || request_size < LAST_CHECK_SIZE)
    LAST_CHECK = END;
  LAST_CHECK_SIZE = request_size;
  FREE_CALLED = 0;
  size_t most_space = 0;

  while( !IS_FREE(LAST_CHECK) || GET_SIZE(LAST_CHECK) < request_size){
    //printf("\tCHECKING FREE BLOCKS: %p; SIZE Difference: %d; FREE: %d;\n", LAST_CHECK, GET_SIZE(LAST_CHECK) - request_size, !!IS_FREE(LAST_CHECK));
    if (IS_FREE(LAST_CHECK) && GET_SIZE(LAST_CHECK) > most_space){
      most_space = GET_SIZE(LAST_CHECK);
    }
    if(LAST_CHECK == BASE){
      // printf("\t\tNo Free BLOCK, MARGIN: %lu\n", most_space - request_size);
      return NULL;
    }
    LAST_CHECK = GET_PREVIOUS_BLOCK(LAST_CHECK);
  }
  // printf("\t\tBLOCK: %p, IS FREE: %d, MARGIN: %lu\n", LAST_CHECK, !!IS_FREE(LAST_CHECK), GET_SIZE(LAST_CHECK) - request_size);

  return LAST_CHECK;

}

int check_unique(block_node tocheck){
  block_node current = END;
  while(current != NULL) {
    if (tocheck == current){
      return 0;
    }
    current = GET_PREVIOUS_BLOCK(current);
  }
  return 1;
}

// //TODO add end of heap footer
// //Always assume that if this function is called, block is at end of list
block_node request_space(block_node last, size_t size){
  // printf("\tIN request_space LAST: %p\n", last );
  block_node block;
  block = mem_sbrk(0);
  void * request = mem_sbrk(GET_BLOCK_SIZE(size));
  if(request == (void *)-1)
    return NULL;
  // printf("%p; %p; %p\n", block, request, mem_sbrk(0));
  assert(check_unique(block)); //WHY DOES this fail.
  assert(check_unique(request));
  assert(request != NULL);
  assert((int)request %ALIGNMENT == 0);
  if(last){
    SET_PREVIOUS_BLOCK(block, last);
  }
  size = ALIGN(size);
  SET_SIZE(block, size);
  SET_FREE(block, 1);
  assert((int)block % ALIGNMENT == 0);
  //*(block_node *)block = NULL; //next is null
  //// printf("END: %p, BLOCK: %p\n",END, block );
  END = block;
  //// printf("END: %p\n", *(block_node*)END);
  // printf("\t\tBLOCK: %p, IS GRANTED: \n",block );

  return block;
}


block_node split_block(block_node block, size_t cutoff){
  // printf("\tIN split_block\n" );
  cutoff = ALIGN(cutoff);
  int isEnd = (block == END);
  block_node next_block = NULL;
  if (!isEnd){
    next_block = GET_NEXT_BLOCK(block);
  }
  size_t old_size = GET_SIZE(block);
  SET_SIZE(block, cutoff);
  block_node s_block = GET_NEXT_BLOCK(block);
  SET_PREVIOUS_BLOCK(s_block, block);
  SET_FREE(s_block, 1);
  SET_SIZE(s_block, old_size-cutoff-BLOCK_HEADER_SIZE);

  if(!isEnd){
    SET_PREVIOUS_BLOCK(next_block, s_block);
  }else{
    END = s_block;
  }
  // printf("\t\tBLOCK: %p, IS SPLIT INTO BLOCKS %p AND %p\n",block, block, s_block);
  return block;
}

void *mm_malloc(size_t size)
{
  // printf("IN mm_malloc, SIZE = %#lx\n", size );

  block_node block;
  if (size <= 0){
    return NULL;
  }
  add_request(size);
  if (!BASE){
    //Frist call
    block = request_space(NULL, size);
    if(!block){
      return NULL;
    }
    SET_FREE(block, 0);
    BASE = block;
  }else{
    block = find_free(size);
    //mm_check();
    if(!block){
      //// printf("No FREE block\n");
      //get space
      block = request_space(END, size);
      if(!block){
        return NULL;
      }
      SET_FREE(block, 0);

    }else{
      //// printf("FREE BLOCK");
      assert((int)block % ALIGNMENT == 0);
      //check split
      if(GET_SIZE(block) > GET_BLOCK_SIZE(size) + GET_BLOCK_SIZE((int)AVERAGE_REQUEST_SIZE)){
        // printf("\tsplitting FREE Block, MARGIN: %lu, running average: %#x \n", GET_SIZE(block)- GET_BLOCK_SIZE(size), AVERAGE_REQUEST_SIZE);
        block = split_block(block, size);
      }
      SET_FREE(block, 0);
    }
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
 * mm_free - Freeing a block does nothing.
 Always merge free blocks
 */
void mm_free(void *ptr)
{
  // printf("IN mm_free\n" );

  if(!ptr || ((int)ptr%ALIGNMENT != 0)){
    return;
  }
  FREE_CALLED = 1;//need for find free

  block_node block = GET_HEADER(ptr);
  assert((int)block%8 == 0 );
  SET_FREE(block, 1);
  // printf("\tFreeing BLOCK: %p\n", block);

  block_node prev = NULL;
  if(block != BASE)
    prev = GET_PREVIOUS_BLOCK(block);

  block_node next= NULL;
  if(block != END)
    next = GET_NEXT_BLOCK(block);

  if(block != BASE && prev && IS_FREE(prev)){
    //double check that this previous block is actually adjacent
    if(GET_NEXT_BLOCK(prev) == block){

      size_t prev_size = GET_SIZE(prev);
      prev_size += GET_SIZE(block) + BLOCK_HEADER_SIZE;
      assert(GET_NEXT_BLOCK(prev) == block);
      SET_SIZE(prev, prev_size);
      if(block != END){
        SET_PREVIOUS_BLOCK(next, prev);
      }else{
        END = prev;
      }
      // printf("\tBLOCK: %p, has been merged left into BLOCK: %p\n",block, prev );

      block = prev;
    }
  }
  if(block != END && next && IS_FREE(next)){
    size_t block_size = GET_SIZE(block);
    block_size += GET_SIZE(next) + BLOCK_HEADER_SIZE;
    SET_SIZE(block, block_size );
    if (next == END){
      END = block;
    }else{
      SET_PREVIOUS_BLOCK(GET_NEXT_BLOCK(next), block);
    }
    // printf("\tBLOCK: %p, has been merged right into BLOCK: %p\n",block, next );
  }

  #ifdef DEBUG
  #ifndef NDEBUG
    mm_check();
  #endif
  #endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  // printf("IN mm_realloc\n" );

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
