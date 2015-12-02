/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
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
 * void * previous
 * size_t size (union with 1 bit active)
 * (allocated data)
 */
typedef void * block_node;
#define BLOCK_HEADER_SIZE ALIGN((sizeof(block_node) + sizeof(size_t)))

#define GET_BLOCK_SIZE(size) (ALIGN((size) + BLOCK_HEADER_SIZE))
#define GET_HEADER(dataptr) ((block_node)((char*)dataptr - BLOCK_HEADER_SIZE))
#define GET_DATA(blockptr) ((void*)((char*)blockptr + BLOCK_HEADER_SIZE))

#define GET_PREVIOUS_BLOCK(currentBlock) *(block_node*)currentBlock
#define GET_NEXT_BLOCK(currentBlock) (block_node)((char*)currentBlock + BLOCK_HEADER_SIZE + GET_SIZE(currentBlock))

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
block_node* BASE = NULL;
block_node* END = NULL;
block_node* LAST_CHECK = NULL;
size_t LAST_CHECK_SIZE = 0;

int NUM_REQUEST = 0;
int AVERAGE_REQUEST_SIZE = 0;
#define ADD_REQUEST(size) (AVERAGE_REQUEST_SIZE += ALIGN(size)/(++NUM_REQUEST));

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
  *(block_node*)testNode = (block_node)0xbee71e5;
  SET_SIZE(testNode, ALIGN(testNodeSize));
  // should properly find the next and previous blocks
  assert(GET_NEXT_BLOCK(testNode) == (char*)testNode + BLOCK_HEADER_SIZE + ALIGN(testNodeSize));
  assert(GET_PREVIOUS_BLOCK(testNode) == (block_node)0xbee71e5);

  printf("Macros checked successfully!\n");

  return 1;
}

/*
 * returns non-zero if macros function properly
 */
int mm_check(void) 
{
  int has_failed = 0;
  printf("\tBASE: %p; END: %p\n", BASE, END);

  block_node* current = BASE;
  block_node* last = NULL;
  while(current != NULL & current < END) {
    printf("\t\tCHECKING: %p; SIZE: %#lx; FREE: %d\n", current, GET_SIZE(current), !!IS_FREE(current));

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

    last = current;
    current = GET_NEXT_BLOCK(current);
  }

  // overlapping blocks?

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
  // TODO remove call from final code.
  if(!macro_checker()) {
    return 0;
  }

  mm_check();

  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */


// //Go 1 past, and check the size of previous, best fit reduces fragmentation
// //TODO add implment split
// //ALWAYS SPLIT
block_node find_free(size_t request_size){
  request_size = ALIGN(request_size);
  block_node current = LAST_CHECK;
  if(request_size < LAST_CHECK_SIZE){
    current = END;
  }
  while(current && GET_SIZE(current) < request_size){
    current = *(block_node *)current;
  }
  if(current == NULL)
    return current;

  LAST_CHECK = current;
  LAST_CHECK_SIZE = request_size;
  return current;

//   block_header *last =NULL;
//   block_header *current = LAST_CHECK;
//   if(request_size < LAST_CHECK_SIZE && LAST_CHECK){
//     current = BASE;
//   }
//   do{
//     last = current;
//     current = *current;
//     assert(last == get_prev(current)); //Maintain inorder of list
//     last_size = get_prev_size(current);
//     if (last_size >= request_size && ISFREE(last_size)){
//       LAST_CHECK_SIZE = request_size;
//       LAST_CHECK = last;
//       return last;
//     }
//     return NULL;
//   }while(current); //End of list should be null
  return NULL;
}

// //TODO add end of heap footer
// //Always assume that if this function is called, block is at end of list
block_node request_space(block_node* last, size_t size){
  block_node block;
  block = mem_sbrk(0);
  void * request = mem_sbrk(GET_BLOCK_SIZE(size));
  if(request == (void *)-1)
    return NULL;
  assert((int)request %ALIGNMENT == 0);
  if(last){
    *last = block;
  }
  size = ALIGN(size) | FREE_MASK;
  SET_SIZE(block, size);
  assert((int)block % ALIGNMENT == 0);
  *(block_node *)block = NULL; //next is null
  END = block;
  return block;
}

//   block_header* block;
//   block = mem_sbrk(0);
//   size_t newsize = ALIGN(size);
//   void * request = sbrk(newsize + TOTAL_HEADER_SIZE);
//   if (request == (void *)-1)
//     return NULL;
//   assert(request % 8 == 0) //check 8 byte allignment
//   if(last){
//     set_next(last, block);

//   size = size | FREE
//   *((size_t *)((char*)block+newsize+BLOCK_HEADER_SIZE)) = newsize;//Sets the last word to size
//   assert(((char*)block+newsize+BLOCK_HEADER_SIZE) % 8 == 0); //check 8 byte allignment

//   set_next(block, NULL);
//   END = block;
//   return block;
// }


void *mm_malloc(size_t size)
{
  block_node block;
  if (size <= 0){
    return NULL;
  }
  ADD_REQUEST(size)
  if (!BASE){
    //Frist call
    block = request_space(NULL, size);
    if(!block){
      return NULL;
    }
    BASE = block;
  }else{
    block = find_free(size);
    if(!block){
      //get space
      block = request_space(END, size);
      if(!block){
        return NULL;
      }
    }else{
      assert((int)block % ALIGNMENT == 0);
      size = GET_SIZE(block);
      SET_SIZE(block, size);
    }
  }

  mm_check();

  return GET_DATA(block);

  // return (void *)((char *)candidate + BLOCK_HEADER_SIZE);
  //old
 //    int newsize = ALIGN(size + SIZE_T_SIZE);
 //    void *p = mem_sbrk(newsize);
 //    if (p == (void *)-1)
	// return NULL;
 //    else {
 //        *(size_t *)p = size;
 //        return (void *)((char *)p + SIZE_T_SIZE);
 //    }
}

/*
 * mm_free - Freeing a block does nothing.
 Always merge free blocks
 */
void mm_free(void *ptr)
{
  if(!ptr || ((int)ptr%ALIGNMENT != 0)){
    return;
  }

  block_node block = GET_HEADER(ptr);
  block_node prev = GET_PREVIOUS_BLOCK(block);
  block_node next;
  if(block != END)
    next = GET_NEXT_BLOCK(block);
  if(IS_FREE(prev)){
    size_t prev_size = GET_SIZE(prev);
    prev_size += GET_SIZE(block) + BLOCK_HEADER_SIZE;
    assert(GET_NEXT_BLOCK(prev) == block);
    SET_SIZE(prev, prev_size | FREE_MASK);
    if(block != END){
      *(block_node*)next = prev;
    }
    block = prev;
  }
  if(block != END && IS_FREE(next)){
    size_t block_size = GET_SIZE(block);
    block_size += GET_SIZE(next) + BLOCK_HEADER_SIZE;
    SET_SIZE(block, block_size | FREE_MASK);
  }

  mm_check();

  // //Check null pointer
  // if (!ptr){
  //   return
  // }
  // //assume valid pointer
  // assert(ptr % 8 == 0) //check 8 byte allignment
  // //coalese first
  // block = get_block_header(ptr);
  // block_header* prev = get_prev(ptr);
  // block_header* next = *block;
  // size_t * block_size_ptr = get_prev_size_ptr(next);
  // size_t block_size = get_prev_size(next);

  // //coalese left
  // if(ISFREE(*get_prev_size_ptr(block))){
  //   size_t prev_size = get_prev_size(block);
  //   prev_size += block_size+TOTAL_HEADER_SIZE;
  //   set_next(prev, next);
  //   *block_size_ptr = prev_size | FREE;
  // }
  // //coalese right
  // if(ISFREE(*get_prev_size_ptr((*block_header)*next))){

  // }

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
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
