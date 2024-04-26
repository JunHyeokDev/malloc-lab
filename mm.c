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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "TeamA",
    /* First member's full name */
    "JunHyeok,Kim",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

static char* heap_listp;


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4 // word and header size (bytes)
#define DSIZE 8 // Double word size 
#define CHUNKSIZE (1<<12) // Extend heap by this amount when it is needed (bytes)!

/* 메모리 블록의 크기(Size)와 할당 여부(alloc)를 비트 단위로 조합한 값입니다.  */
/* combines a size and an allocate bit and returns a value that can be stored in a header or footer.*/
#define PACK(size,alloc) ((size) | (alloc)) 

/* The GET reads and returns the word referenced by argu- ment p.*/
/* The casting here is crucial. The argument p is typically a (void *) pointer, which cannot be dereferenced directly. */
/* p를 (unsigned int *) 포인터 타입으로 캐스팅 한 뒤 값을 역참조 하겠다는 뜻 입니다! */
#define GET(p) (*(unsigned int *)(p)) 
/* 단순히 해당 위치의 값을 val로 넣는 것 입니다! */
#define PUT(p,val) (*(unsigned int *)(p) = (val)) 

#define GET_SIZE(p)  (GET(p) & ~0x7) /* p 10진수 크기 값을 반환합니다*/ 
#define GET_ALLOC(p) (GET(p) & 0x1)  /* p 의 할당 여부를 반환합니다*/ 

#define HDRP(bp) ((char *)(bp) - WSIZE) /* HDRP 는 Header Pointer의 약자입니다 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* HDRP 는 Header Pointer의 약자입니다 */

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-DSIZE)))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */

static void* extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* 짝수면 words % 2 가 홀수가 나오겟지? 그러므로 words+1이 실행됨 */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long) (bp = (char*)mem_sbrk(size)) == -1) { // bp할당 
        return NULL;
    }
    PUT(HDRP(bp), PACK(size,0));  // 해당 위치 Header Pointer 값 설정
    PUT(FTRP(bp), PACK(size,0));  // 해당 위치 Footer Pointer 값 설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // 에필로그는 크기가 0이라고 했지.
    // printf("HDRP : %p\n", HDRP(bp));
    // printf("FTRP : %p\n", FTRP(bp));
    // printf("Epil : %p\n", HDRP(NEXT_BLKP(bp)));

    return NULL;
}

int mm_init(void)
{

    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*) -1 ) {
        return -1;
    }
    PUT(heap_listp,0); // Alignment padding
    PUT(heap_listp + (1*WSIZE),  PACK(DSIZE,1)); // Prologue header
    PUT(heap_listp + (2*WSIZE),  PACK(DSIZE,1)); // Prologue footer
    PUT(heap_listp + (3*WSIZE),  PACK(0,1));     // Epilogue header
    printf("start !!!!\n");
    printf("heap_listp  : %p\n",heap_listp);
    printf("HDRP : %p\n", heap_listp + (1*WSIZE));
    printf("FTRP : %p\n", heap_listp + (2*WSIZE));
    printf("Epil : %p\n", heap_listp + (3*WSIZE));
    heap_listp += (2*WSIZE);
    printf("heap_listp  : %p\n",heap_listp);
    printf("end !!!!\n");
    extend_heap(CHUNKSIZE/WSIZE);
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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














