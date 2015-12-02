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

#define GET_PREVIOUS_BLOCK(currentBlock) (block_node)*currentBlock
#define GET_NEXT_BLOCK(currentBlock) (block_node)((char*)currentBlock + GET_SIZE(currentBlock))

#define FREE_MASK (1<<(sizeof(size_t) - 1))
#define GET_MASKED_SIZE_POINTER(blockPointer) ((size_t*)((char*)blockPointer + sizeof(block_node)))
#define GET_MASKED_SIZE(blockPointer) (*GET_MASKED_SIZE_POINTER(blockPointer))
#define IS_FREE(blockPointer) (GET_MASKED_SIZE(blockPointer) & FREE_MASK)
#define GET_SIZE(blockPointer) (GET_MASKED_SIZE(blockPointer) & ~FREE_MASK)
#define SET_SIZE(blockPointer, size) ((GET_MASKED_SIZE(blockPointer)) = size)


int macro_checker() {
  // verify block_header_size, should be 8 bytes
  // b/c sizeof(void*) = 4, sizeof(size_t) = 4
  // 4 + 4 = 8
  assert(sizeof(void*) == 4);
  assert(sizeof(size_t) == 4);
  assert(BLOCK_HEADER_SIZE == 8);

  // verify header and data getters
  // should be inverses
  block_node testNode = malloc(GET_BLOCK_SIZE(8));
  assert(GET_HEADER(GET_DATA(testNode)) == testNode);
  // should move the right amount (ie BLOCK_HEADER_SIZE)
  // because: o o o o o o o o
  // assert(GET_DATA(testNode) == testNode + )

  // printf("block size: %lu\n", GET_BLOCK_SIZE(0));
  // printf("block size: %lu\n", GET_BLOCK_SIZE(4));
  // printf("block size: %lu\n", GET_BLOCK_SIZE(8));
  // printf("block size: %lu\n", GET_BLOCK_SIZE(300));
  return 0;
}


// //Free list Stuff
block_header* BASE=NULL;
block_header* END = NULL;
block_header * LAST_CHECK = NULL
size_t LAST_CHECK_SIZE = 0

/*
 * mm_init - initialize the malloc package.
 TODO check for multiple calls
 */
int mm_init(void)
{
  macro_checker();

  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

// //Cannot call with BASE
// inline block_header* get_prev(block_header* block){
//   size_t size = GETSIZE(*(size_t*)((char *)block -SIZE_T_SIZE))
//   assert((char *)block -SIZE_T_SIZE % 8 == 0) //check 8 byte allignment
//   prev = (block_header*)((char *)block - size - TOTAL_HEADER_SIZE);
//   assert(prev % 8 == 0) //check 8 byte allignment
//   return prev;
// }
// inline block_header* get_next(block_header* block){
//   block_header* next = *block;
//   assert(next & 8 == 0);
//   return next;
// }

// inline size_t* get_size_ptr(block_header* block){
//   block_header* next = *block;
//   size_t* size = (size_t*)((char *)block -SIZE_T_SIZE);
//   assert(size % 8 == 0) //check 8 byte allignment
//   return size;
// }

// inline size_t get_prev_size(block_header* block){
//   size_t size = GETSIZE(*(size_t*)((char *)block -SIZE_T_SIZE));
//   assert((char *)block -SIZE_T_SIZE % 8 == 0) //check 8 byte allignment
//   return size;
// }


// inline size_t get_prev_size_ptr(block_header* block){
//   size_t size = (size_t*)((char *)block -SIZE_T_SIZE);
//   assert(size % 8 == 0) //check 8 byte allignment
//   return size;
// }

// inline block_header* get_block_header(void *ptr){
//   return (block_header*)(ptr - BLOCK_HEADER_SIZE);
// }

// inline void* get_data(block_header* block){
//   return (void*)((char*)block + BLOCK_HEADER_SIZE);
// }

// inline void set_next(block_header* block, block_header next){
//   *block = next;
// }

// //set free
// inline void set_free(block_header* block, int should_free){
//   if(should_free){

//   }
// }
// //Go 1 past, and check the size of previous, best fit reduces fragmentation
// //TODO add implment split
// //ALWAYS SPLIT
block_header find_free(size_t request_size){
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
  return NULL
}

// //TODO add end of heap footer
// //Always assume that if this function is called, block is at end of list
block_node request_space(block_node* last, size_t size){
  block_node block;
  block = mem_sbrk(0);
  void * request = mem_sbrk(GET_BLOCK_SIZE(size));
  if(request == (void *)-1)
    return NULL;
  assert(request %ALIGNMENT == 0);
  if(last){
    *last = block;
  }
  size = size | FREE_MASK;
  SET_SIZE(block, size);
  assert(block %ALIGNMENT == 0);
  block = NULL; //next is null
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
}

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
      assert(block %ALIGNMENT == 0);
      size = GET_SIZE(block) & ~FREE_MASK;
      SET_SIZE(block, size);
    }
  }

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
