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
    "team 8",
    /* First member's full name */
    "D_corn",
    /* First member's email address */
    "dcron@jungle.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Basic constants and macros
#define WSIZE 4 // 워드 = 헤더 = 풋터 사이즈(bytes)
#define DSIZE 8 // 더블워드 사이즈(bytes)
#define CHUNKSIZE (1<<12) // heap을 이정도 늘린다(bytes)
#define LISTLIMIT 20 //list의 limit 값을 설정해준다. CHUNKSIZE에 비해 충분히 큰 값을 준 것 같다(정확한 이유 모르겠음)

#define MAX(x, y) ((x) > (y)? (x):(y))
// pack a size and allocated bit into a word 
#define PACK(size, alloc) ((size) | (alloc))

// Read and wirte a word at address p
//p는 (void*)포인터이며, 이것은 직접 역참조할 수 없다.
#define GET(p)     (*(unsigned int *)(p)) //p가 가리키는 놈의 값을 가져온다
#define PUT(p,val) (*(unsigned int *)(p) = (val)) //p가 가리키는 포인터에 val을 넣는다

// Read the size and allocated fields from address p 
#define GET_SIZE(p)  (GET(p) & ~0x7) // ~0x00000111 -> 0x11111000(얘와 and연산하면 size나옴)
#define GET_ALLOC(p) (GET(p) & 0x1)  // 할당이면 1, 가용이면 0

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //헤더+데이터+풋터 -(헤더+풋터)

// Given block ptr bp, compute address of next and previous blocks
// 현재 bp에서 WSIZE를 빼서 header를 가리키게 하고, header에서 get size를 한다.
// 그럼 현재 블록 크기를 return하고(헤더+데이터+풋터), 그걸 현재 bp에 더하면 next_bp나옴
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PRED_FREE(bp) (*(void**)(bp))
#define SUCC_FREE(bp) (*(void**)(bp + WSIZE))


static void *heap_listp;          
static void *segregation_list[LISTLIMIT];

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void remove_block(void *bp);
static void insert_block(void *bp, size_t size);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int list;

    for (list = 0; list < LISTLIMIT; list++) {
        segregation_list[list] = NULL;     // segregation_list를 NULL로 초기화
    }                                      // 나중에 그 list에는 값이 없음을 표시할 수 있도록. 

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);                            // Unused padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 헤더 8/1
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 풋터 8/1
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // 에필로그 헤더 0/1
    heap_listp = heap_listp + 2*WSIZE;  //왜 프롤로그 사이를 가리키고 있지? 
    //find_fit 함수에서 find를 할 때 사용하기 위해서
    /* Extended the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int asize = ALIGN(size + SIZE_T_SIZE);

    // size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) // 가용 블록을 찾을 수 있다면
    {
        place(bp, asize);               // place 함수를 실행시킴
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
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
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    // memcpy - 메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로
    // 복사해주는 함수(oldptr로부터 copySize만큼의 문자를 newptr로 복사해라)
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    //coalesce는 succ, pred를 보는 것이 아니라 인접한 prev, next 블록을 보는 것을 주의!!!!!!!!!!
    //전 블록 가용한지
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    //다음 블록 가용한지
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){ // 앞,뒤 모두 allocated
        insert_block(bp,size);      
        return bp;
    }
    else if (prev_alloc && !next_alloc) // 앞은 allocated, 바로 뒤에 free block이 존재할 때
    {
        remove_block(NEXT_BLKP(bp)); // 뒤 블록을 일단 지워(나중에 합쳐서 insert 해줄거야)
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 합친 블록 사이즈
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) // 앞은 free block, 뒤는 allocated
    {
        remove_block(PREV_BLKP(bp)); // 앞 블록을 일단 지워(나중에 합체하고 insert 해줄거야)

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // 앞으로 bp 옮겨줘야함
    }
    else if (!prev_alloc && !next_alloc) // 앞과 뒤 모두 free block일 경우
    {
        remove_block(PREV_BLKP(bp)); // 다 지워
        remove_block(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // 앞으로 bp 옮겨줘야함
    }

    insert_block(bp, size); // 드디어 insert! (coalesce를 하면 무조건 insert가 실행됨)
    return bp;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    // allocate된 블록은 freelist에서 지운다.
    remove_block(bp);
    // 필요한 블록 이외에 남는게 16바이트 이상이면 - (header,pred,succ,footer 각각 16byte필요)
    if ((csize - asize) >= (2 * DSIZE)) //분할을 진행한다.
    {   // 일단 할당블록 처리
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); //bp를 다음 블록으로 옮김
        // 나머지 사이즈를 free시킨다.
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp); // 이때 연결되어 있는 게 있을 수 있으므로 coalesce진행
    }                 // coalesce 함수에 들어가면 무조건 insert를 하게 됨
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void *find_fit(size_t asize)
{
    /* First-fit search */
    void *bp;

    int list = 0;
    size_t searchsize = asize;

    while (list < LISTLIMIT){
        // (list가 현재 0~19이므로)가용블록을 못찾아서 19번째 리스트에 도달하거나,
        // (19번째 list에는 무조건 넣어야 함)
        // 나보다 큰 사이즈의 segregation_list가 NULL이 아니면 (나보다 큰 사이즈의 list 안에 free 블록이 존재할 경우)
        if ((list == LISTLIMIT-1) || (searchsize <= 1)&&(segregation_list[list] != NULL)){
            bp = segregation_list[list];

            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp)))){
                bp = SUCC_FREE(bp);
            }
            if (bp != NULL){
                return bp;
            }
        }
        searchsize >>= 1;
        list++;
    }

    return NULL; /* no fit */

// #endif
}

static void remove_block(void *bp){
    int list = 0;
    size_t size = GET_SIZE(HDRP(bp));

    while ((list < LISTLIMIT - 1) && (size > 1)) { //지우고자 하는 list idx 찾아들어감
        size >>= 1;
        list++;
    }

    if (SUCC_FREE(bp) != NULL){ // succ 블록이 NULL이 아니면
        if (PRED_FREE(bp) != NULL){ // pred 블록이 NULL이 아니면 (중간에 있는걸 지우는 경우)
            PRED_FREE(SUCC_FREE(bp)) = PRED_FREE(bp);
            SUCC_FREE(PRED_FREE(bp)) = SUCC_FREE(bp);
        }else{ // pred 블록이 NULL일 경우 (list에서 맨 처음을 지우는 경우)
            PRED_FREE(SUCC_FREE(bp)) = NULL;
            segregation_list[list] = SUCC_FREE(bp);
        }
    }else{ //succ 블록이 NULL일 경우
        if (PRED_FREE(bp) != NULL){ //리스트의 끝의 블록을 지우는 경우
            SUCC_FREE(PRED_FREE(bp)) = NULL;
        }else{ // 애초에 하나만 존재했을 경우
            segregation_list[list] = NULL;
        }
    }
    return;
}

static void insert_block(void *bp, size_t size){
    int list = 0;
    void *search_ptr;
    void *insert_ptr = NULL; //search_ptr의 값을 저장해놓는 용도(insert_ptr의 부모같음)

    while ((list < LISTLIMIT - 1) && (size > 1)){ //segregation_list의 idx를 찾는 과정
        size >>=1;
        list++;
    }

    search_ptr = segregation_list[list];
    //오름차순으로 저장하기 위해 나보다 작은 놈들은 넘기고 나보다 큰놈 앞에서 멈추게 됨
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr)))){
        insert_ptr = search_ptr;
        search_ptr = SUCC_FREE(search_ptr); //succ로 계속 넘어가서 찾는다.
    }

    if (search_ptr != NULL){ //search_ptr이 NULL이 아닐 때
        if (insert_ptr != NULL){ //insert_ptr이 NULL이 아닐 때
            SUCC_FREE(bp) = search_ptr; //insert, search 사이에 넣는 경우
            PRED_FREE(bp) = insert_ptr;
            PRED_FREE(search_ptr) = bp;
            SUCC_FREE(insert_ptr) = bp;
        }else{ //insert_ptr이 NULL일 때 (안에 들어왔는데 내가 제일 작아서 list에 바로 삽입할 때)
            SUCC_FREE(bp) = search_ptr;
            PRED_FREE(bp) = NULL;
            PRED_FREE(search_ptr) = bp;
            segregation_list[list] = bp; //segregation_list 최신화
        }
    }else{ // search_ptr이 NULL일 때
        if (insert_ptr != NULL){        // 처음 시작할 때는 이 코드가 돌아갈 일이 없지만
            SUCC_FREE(bp) = NULL;       // 진행하다보면 연결 list안에서 내가 제일 커서 search_ptr은 null, 
            PRED_FREE(bp) = insert_ptr; // insert_ptr은 현재 list에서 가장 큰 경우가 존재한다.
            SUCC_FREE(insert_ptr) = bp;
        }else{ // 아무것도 없어서 list에 내가 처음 넣을 때
            SUCC_FREE(bp) = NULL;
            PRED_FREE(bp) = NULL;
            segregation_list[list] = bp; //segregation_list 최신화
        }
    }

    return;
}