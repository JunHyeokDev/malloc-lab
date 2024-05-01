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
    "JunHyeok Team",
    /* First member's full name */
    "JunHyeok KIM",
    /* First member's email address */
    "fixme1537@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t allocated_size);
static void *extend_heap(size_t words);
static void remove_free_block(void *bp);
static void add_free_block(void *bp);

#define LISTLIMIT 20
static void *seg_list[LISTLIMIT];

// Basic constants and macros
#define WSIZE               4                                                       // word = 4 byte
#define DSIZE               8                                                       // double word = 8 byte
#define CHUNKSIZE           (1 << 12)                                               // chunk size = 2^12 = 4096

#define MAX(x, y)           ((x) > (y) ? (x) : (y))                                 // x와 y 중 큰 값을 반환

// Pack a size and allocated bit into a word
#define PACK(size, alloc)   ((size) | (alloc))                                      // 크기 비트와 할당 비트를 통합해 header 및 footer에 저장할 수 있는 값 리턴

// Read and write a word at address p
#define GET(p)              (*(unsigned int *)(p))                                  // p가 참조하는 word를 읽어서 리턴 ((void*)라서 type casting 필요)
#define PUT(p, val)         (*(unsigned int *)(p) = (val))                          // p가 가리키는 word에 val 저장

// Read the size and allocated fields from address p
#define GET_SIZE(p)         (GET(p) & ~0x7)                                         // 사이즈 (뒤에 세자리 없어짐 -> 할당 여부와 무관한 사이즈를 반환)
#define GET_ALLOC(p)        (GET(p) & 0x1)                                          // 할당 여부 (마지막 한자리, 즉 할당 여부만 가지고 옴)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp)            ((char *)(bp) - WSIZE)                                  // 해당 블록의 header 주소를 반환 (payload 시작점인 bp에서 헤더 크기인 1 word를 뺀 값)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)             // 해당 블록의 footer 주소를 반환 (payload 시작점인 bp에서 블록 사이즈를 더한 뒤 2 word를 뺀 값)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))       // 다음 블록의 bp를 반환 (현재 bp에서 해당 블록의 크기를 더해준 값)
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))       // 이전 블록의 bp를 반환 (현재 bp에서 이전 블록의 크기를 빼준 값 -> DSIZE를 빼야 이전 블록의 정보를 가져올 수 있음!!)

// Find Successor and Find Predecessor
#define FIND_SUCC(bp)       (*(void**)(bp+WSIZE))               // 후임자 (Next) 찾기
#define FIND_PRED(bp)       (*(void**)(bp))                     // 전임자 (Prev) 찾기

/* single word (4) or double word (8) alignment */
// 정렬할 크기
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT
* - 입력된 size에 ALIGNMENT-1(111)을 더함
* - 0x7: 0000 0111 ~0x7: 1111 1000
* - & 연산자로 마지막 세자리를 0으로 바꿈
* -> size에 가장 가까운 8배수 생성
*/
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 추가 변수 선언
static char *heap_listp = NULL;         // 메모리 힙의 시작 주소
static char *free_listp = NULL;         // free_list에 대한 주소

/* 
 * mm_init - initialize the malloc package.
 * - 초기 힙 영역을 확보하고 블록 생성하는 함수
 * - 할당기를 초기화하고 성공이면 0, 아니면 -1 리턴
 * - sbrk 함수를 호출하여 힙 영역 확보
 * - 프롤로그 블록: header, footer로 구성된 8 byte 할당 블록 
 * - 에필로그 블록: header로 구성된 0 byte 할당 블록
 */
int mm_init(void)
{
    int idx;
    for (idx = 0; idx < LISTLIMIT; idx++) {
        seg_list[idx] = NULL;
    }
    
    // 초기 힙 생성
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) // 8워드 크기의 힙 생성, free_listp에 힙의 시작 주소값 할당(가용 블록만 추적)
        return -1;
        PUT(heap_listp, 0);                                         // Padding (8의 배수로 정렬하기 위해 패딩 블록 할당)
        PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));              // Prologue Header
        PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));              // Prologue Footer
        PUT(heap_listp + (3 * WSIZE), PACK(0, 1));                  // Epilogue Header

    heap_listp += 2*DSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}
/*
* extend_heap - brk 포인터로 힙 영역을 확장하는 함수 (확장 단위는 CHUNK SIZE)
* - mm_init에서 초기 힙 영역을 확보했다면, 블록이 추가될 때 힙 영역을 확장해주는 역할
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;       // 워드 사이즈가 홀수라면 1을 더하고 짝수라면 그대로 WSIZE를 곱해서 사이즈 변환
    if ((bp = mem_sbrk(size)) == (void *)-1)                        // mem_sbrk로 힙 확보가 가능한지 여부를 체크
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));                                   // Block Header -> free(0) 설정
    PUT(FTRP(bp), PACK(size, 0));                                   // Block Footer -> free(0) 설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));                           // New Epilogue Header

    return coalesce(bp);                                            // 블록 연결 함수 호출
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *     요청한 사이즈만큼 메모리 블록을 가용 블록으로 확보하고 해당 payload 주소값을 리턴하는 함수
 */ 
void *mm_malloc(size_t size)
{
    size_t allocated_size;      // 블록 사이즈 조정
    size_t extend_size;         // heap에 맞는 fit이 없다면 확장하기 위한 사이즈
    void *bp;

    if (size <= 0)
        return NULL;
    
    if (size <= DSIZE)                                                      // 할당할 크기가 8 byte보다 작으면 allocated size에 16 byte를 담음
        allocated_size = 2 * DSIZE;
    else                                                                    // 할당할 크기가 8 byte보다 크면 allocated size를 8의 배수로 맞춰줘야 함
        allocated_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);    // (할당할 크기 + 8 bytes (Header + Footer) + 7)을 8로 나눈 몫 (8의 배수로 맞춰주는 작업)
    
    if ((bp = find_fit(allocated_size)) != NULL)                            // find_fit 함수로 할당된 크기를 넣을 공간이 있는지 체크
    {
        place(bp, allocated_size);
        return bp;
    }

    extend_size = MAX(allocated_size, CHUNKSIZE);                           // 넣을 공간이 없다면, 새 가용블록으로 힙을 확장
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL)                    // 확장할 공간이 없다면 NULL 반환
        return NULL;
    
    place(bp, allocated_size);                                              // 확장이 됐다면, 공간을 할당하고 분할
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));           // 해당 블록의 사이즈 저장
    PUT(HDRP(bp), PACK(size, 0));               // 해당 블록의 Header -> free(0) 설정
    PUT(FTRP(bp), PACK(size, 0));               // 해당 블록의 Footer -> free(0) 설정
    coalesce(bp);                               // free 블록 연결 함수 호출
}
/*
 * add_free_block - 프리 블록을 연결리스트 제일 첫 번째에 넣어줍니다.
 */
static void add_free_block(void *bp) { 
    int idx = 0;
    void *search_ptr;
    void *insert_ptr = NULL;
    size_t size = GET_SIZE(HDRP(bp));

    // free 블록을 seg_list에 넣기 위해 idx를 찾습니다.
    while ((idx < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1; // 2의 배수로 감소!!
        idx++;
    }

    search_ptr = seg_list[idx];
    while ((search_ptr != NULL)) {
        insert_ptr = search_ptr;
        search_ptr = FIND_SUCC(search_ptr);
    }

    if (search_ptr != NULL) { 
        if (insert_ptr != NULL) { 
            // search & insert ptr이 null이 아닌 경우에는 중간에 끼는 경우이다.
            FIND_SUCC(bp) = search_ptr;
            FIND_PRED(bp) = insert_ptr;
            FIND_PRED(search_ptr) = bp;
            FIND_SUCC(insert_ptr) = bp;
        } else {
            // search_ptr 가 NULL이 아니고 insert ptr이 NULL이면 제일 처음인 경우
            FIND_SUCC(bp) = search_ptr;
            FIND_PRED(search_ptr) = bp;
            FIND_PRED(bp) = NULL;
            seg_list[idx] = bp;
        }
    }
    else {
        // search_ptr == NULL
        // insert_ptr != NULL 인 상황.
        if (insert_ptr != NULL) {
            FIND_SUCC(bp) = NULL;
            FIND_SUCC(insert_ptr) = bp;
            FIND_PRED(bp) = insert_ptr;
        }
        // search_ptr == NULL
        // insert_ptr == NULL 인 상황.
        else {
            FIND_SUCC(bp) = NULL;
            FIND_PRED(bp) = NULL;
            seg_list[idx] = bp;
        }
    }
}


/*
* coalesce - 가용 블록 병합 함수
*/
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // Case 1: prev 할당 불가(1) & next 할당 불가(1)
    if (prev_alloc && next_alloc) {
        add_free_block(bp); // free 됐을 때 coalesce가 호출되므로, free 된 block을 연결리스트에 추가 해줍니다.
        return bp;
    }
    // Case 2: prev 할당 불가(1) & next 할당 가능(0) 
    else if (prev_alloc && !next_alloc)
    {
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));      // 현재 블록 사이즈에 다음 블록 사이즈를 더함
        PUT(HDRP(bp), PACK(size, 0));               // 현재 블록의 Header에 새로운 사이즈와 free(0) 설정
        PUT(FTRP(bp), PACK(size, 0));               // 현재 블록의 Footer에 새로운 사이즈와 free(0) 설정
    }

    // Case 3: prev 할당 가능(0) & next 할당 가능(1)
    else if (!prev_alloc && next_alloc)
    {
        remove_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));      // 현재 블록 사이즈에 이전 블록 사이즈를 더함 
        PUT(FTRP(bp), PACK(size, 0));               // 현재 블록의 Footer에 새로운 사이즈와 free(0) 설정
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 이전 블록의 Header에 새로운 사이즈와 free(0) 설정
        bp = PREV_BLKP(bp);                         // bp를 이전 블록의 위치로 변경
    }

    // Case 4: prev 할당 가능(0) & next 할당 가능(0)
    else
    {
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));      // 현재 블록 사이즈에 이전 블록 사이즈와 다음 블록 사이즈를 더함
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                                    // 이전 블록 Header에 새로운 사이즈와 free(0) 설정
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));                                    // 다음 블록 Footer에 새로운 사이즈와 free(0) 설정
        bp = PREV_BLKP(bp);                                                         // bp를 이전 블록의 위치로 변경
    }
    add_free_block(bp);
    return bp;
}

static void remove_free_block(void *bp) {
    int idx = 0;
    size_t size = GET_SIZE(HDRP(bp));

    // size가 1이 될 때 까지 비트 연산을 수행하면 해당 bp가 갖고 size가 seg_list의 어떤 idx에 있는지 알 수 있음.
    while ((idx < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        idx++;
    }

    if (FIND_SUCC(bp) != NULL) { 
        // 양 옆에 free block들이 존재하는 경우
        // 단순히 갈아끼우기 작업에 들어갑시다.
        if (FIND_PRED(bp) != NULL) { 
            FIND_PRED(FIND_SUCC(bp)) = FIND_PRED(bp);
            FIND_SUCC(FIND_PRED(bp)) = FIND_SUCC(bp);
        }
        // SUCC은 존재하나, PRED가 존재하지 않는 경우에는 List의 제일 앞임.
        else {
            FIND_PRED(FIND_SUCC(bp)) = NULL;
            seg_list[idx] = FIND_SUCC(bp); 
        }
    }

    else {
        // bp는 현재 List의 제일 끝에 위치합니다.
        if (FIND_PRED(bp) != NULL){  // bp가 끝에 위치하면서, 첫 번째 요소가 아닌 경우
            FIND_SUCC(FIND_PRED(bp)) = NULL;
        }
        else {
            seg_list[idx] = NULL;
        }
    }
}

/*
* place - 확보된 데이터 블록에 실제로 데이터 사이즈를 반영해서 할당 처리하는 함수
* - 위치와 사이즈를 인자로 받아서 영역 낭비를 최소화
*/
static void place(void *bp, size_t allocated_size)
{
    size_t curr_size = GET_SIZE(HDRP(bp));
    remove_free_block(bp);
    // Case 1. 남는 부분이 최소 블록 크기(16 bytes) 이상일 때 -> 블록 분할
    if ((curr_size - allocated_size) >= (2 * DSIZE))            // 남는 블록이 최소 블록 크기(16 bytes) 이상일 때
    {
        PUT(HDRP(bp), PACK(allocated_size, 1));                 // Header -> size와 allocated(1) 설정
        PUT(FTRP(bp), PACK(allocated_size, 1));                 // Footer -> size와 allocated(1) 설정
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(curr_size - allocated_size, 0));     // 남은 공간의 Header -> 남는 size와 free(0) 설정
        PUT(FTRP(bp), PACK(curr_size - allocated_size, 0));     // 남은 공간의 Footer -> 남는 size와 free(0) 설정
        add_free_block(bp);
    }

    // Case 2. 남는 부분이 최소 블록 크기(16 bytes) 미만일 때 -> 블록 분할 필요 X
    else
    {
        PUT(HDRP(bp), PACK(curr_size, 1));                      // 분할할 필요가 없다면, 그냥 할당
        PUT(FTRP(bp), PACK(curr_size, 1));
    }
}


/*
* find_fit - 할당할 블록을 찾는 함수
* - First Fit, Next Fit, Best Fit 3가지 방법이 있다.
* - 1. First Fit: 처음부터 검색해서 크기가 맞는 첫번째 가용 블록 선택
* - 2. Next Fit: 이전 검색이 종료된 지점부터 검색을 시작하여 크기가 맞는 블록 선택
* - 3. Best Fit: 모든 가용 블록을 검색해서 크기가 맞는 가장 작은 블록 선택
*/

// 1. First Fit Search 
static void *find_fit(size_t asize)
{
    void *bp;
    int idx = 0;
    size_t search_size = asize;

    while (idx < LISTLIMIT) {
        // 만약 search_size가 64라고 해보자, 그러면 1보다 작거나 같아 질 때 까지 아래와 같이 비트연산이 될 것이다.
        // 이에 따라 idx 또한 0부터 차근차근 증가하여 insert 할 공간을 찾을 수 있을 것이다!
        // size : 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1
        // id   : 0  ->  1 ->  2 -> 3 -> 4 -> 5 -> 6
        if ((idx == LISTLIMIT-1) || ( (search_size <= 1)&&(seg_list[idx] != NULL)) ) {
            bp = seg_list[idx];

            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp)))) {
                bp = FIND_SUCC(bp);
            }
            if (bp != NULL) {
                return bp;
            }
        }
        search_size >>= 1;
        idx++;
    }
    return NULL;
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * - 메모리 재할당 함수
 */
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;                        // 새 메모리 블록에 복사해야 할 데이터의 크기
    
//     newptr = mm_malloc(size);               // 요청한 사이즈만큼 블록 할당
//     if (newptr == NULL)
//         return NULL;
    
//     copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;      // 이전 블록 사이즈를 copySize에 저장
    
//     if (size < copySize)                    // 요청한 size가 원래 크기보다 작다면,
//         copySize = size;                    // 기존 메모리 블록에서 일부만 복사해야 하므로 copySize를 요청한 크기로 설정
    
//     memcpy(newptr, oldptr, copySize);       // 이전 블록에서 copySize만큼의 데이터를 새 블록에 복사
//     mm_free(oldptr);                        // 기존 블록 free
//     return newptr;                                          
// }


void *mm_realloc(void *ptr, size_t size)
{   
    if (size == 0) {        // 사이즈가 0인 경우에는 메모리 해제해주고 return NULL;
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL) {      // 주소가 없는 경우 요구에 맞는 size만큼 malloc을 해주면 ptr이 반환됨/
        return mm_malloc(size);
    }

    void *oldptr = ptr;
    void *newptr = oldptr;

    size_t oldsize = GET_SIZE(HDRP(oldptr));
    size_t newsize = ALIGN(size + DSIZE);

    // 이전의 사이즈가 더 큰경우
    if (oldsize >= newsize) {
        size_t rest = oldsize - newsize;
        if (rest >= 2 * DSIZE) {
            // 나머지가 16바이트 이상이면 분할
            PUT(HDRP(oldptr), PACK(newsize, 1));
            PUT(FTRP(oldptr), PACK(newsize, 1));
            oldptr = NEXT_BLKP(oldptr);
            PUT(HDRP(oldptr), PACK(rest, 0));
            PUT(FTRP(oldptr), PACK(rest, 0));
            add_free_block(oldptr);
        }
        return newptr;
    }

    // 더 커지는 경우.
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(oldptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    size_t nextsize = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
    size_t combined_size = oldsize +  nextsize;

    if (!next_alloc && combined_size >= newsize) { // 다음 블록이 할당되지 않고, 합친 사이즈가 할당하려는 new 사이즈보다 크면
        // Coalesce with the next block
        remove_free_block(NEXT_BLKP(oldptr));
        PUT(HDRP(oldptr), PACK(combined_size, 1));
        PUT(FTRP(oldptr), PACK(combined_size, 1));
        return newptr;
    }
    // 만약 다음 블록이 이미 할당 되어 있거나, 새롭게 할당하려는 사이즈가 너무 커서 다음 블록을 사용해도 할당할 수 없다면
    // 그 때서야 새로 할당.
    newptr = mm_malloc(size);
    if (!newptr) {
        return NULL;
    }
    memcpy(newptr, oldptr, oldsize - DSIZE);
    mm_free(oldptr);
    return newptr;
}


/*
void *mm_realloc(void *ptr, size_t size) // 재할당
{
    if (ptr == NULL) // 입력 포인터가 NULL이면, 입력 사이즈만큼 새롭게 할당 (예외처리)
    {
        return mm_malloc(size);
    }
    if (size == 0) // 입력 사이즈가 0이면, 입력 포인터의 블록을 해제 (예외처리)
    {
        mm_free(ptr);
        return NULL;
    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize = GET_SIZE(HDRP(oldptr)); // 재할당하려는 블록의 사이즈
    if (size + DSIZE <= copySize) // (재할당 하려는 블록 사이즈 + 8 bytes(Header + Footer)) <= 현재 블록 사이즈
    {
        return oldptr; // 현재 블록에 재할당해도 문제 없기 때문에, 포인터만 반환
    }
    else // (재할당 하려는 블록 사이즈 + 8 bytes) > 현재 블록 사이즈
         // 경우에 따라서 인접 Free block을 활용하는 방안과, 새롭게 할당하는 방안을 이용해야 함
    {
        size_t next_size = copySize + GET_SIZE(HDRP(NEXT_BLKP(oldptr))); // 현재 블록 사이즈 + 다음 블록 사이즈 = next_size
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(oldptr))); // 이전 블록 사이즈
        if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (size + DSIZE <= next_size))
        // 다음 블록이 Free block이고, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (현재 블록 사이즈 + 다음 블록 사이즈)
        // 현재 블록과 다음 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 얻을 수 있음
        {
            PUT(HDRP(oldptr), PACK(next_size, 1)); // 현재 블록의 Header Block에, (현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            PUT(FTRP(oldptr), PACK(next_size, 1)); // 현재 블록의 Footer Block에, (현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            //lastp = oldptr; // next_fit 사용을 위한 포인터 동기화
            return oldptr;
        }
        else if (!GET_ALLOC(HDRP(PREV_BLKP(oldptr))) && (size + DSIZE <= prev_size + copySize))
        // 이전 블록이 Free block이고, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (이전 블록 사이즈 + 현재 블록 사이즈)
        // 이전 블록과 현재 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 얻을 수 있음
        {
            void *prev_ptr = PREV_BLKP(oldptr); // 이전 블록의 bp
            memmove(prev_ptr, oldptr, copySize); // 이전 블록의 bp로 현재 block의 메모리 영역을 옮긴다
            PUT(HDRP(prev_ptr), PACK(prev_size + copySize, 1)); // 이전 블록의 Header Block에, (이전 블록 사이즈 + 현재 블록 사이즈) 크기와 Allocated 상태 기입
            PUT(FTRP(prev_ptr), PACK(prev_size + copySize, 1)); // 이전 블록의 Footer Block에, (이전 블록 사이즈 + 현재 블록 사이즈) 크기와 Allocated 상태 기입
            //lastp = prev_ptr; // next_fit 사용을 위한 포인터 동기화
            return prev_ptr;
        }
        // else if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && !GET_ALLOC(HDRP(PREV_BLKP(oldptr))) && (size + DSIZE <= next_size + copySize + prev_size))
        // // 이전 블록과 다음 블록이 모두 Free block, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈)
        // // 이전 블록과 현재 블록과 다음 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 어등ㄹ 수 있음
        // {
        //     void *prev_ptr = PREV_BLKP(oldptr); // 이전 블록의 bp
        //     memmove(prev_ptr, oldptr, copySize); // 이전 블록의 bp로 현재 block의 메모리 영역을 옮긴다
        //     PUT(HDRP(prev_ptr), PACK(prev_size + copySize + next_size, 1)); // 이전 블록의 Header Block에, (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
        //     PUT(FTRP(prev_ptr), PACK(prev_size + copySize + next_size, 1)); // 이전 블록의 Footer Block에, (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
        //     lastp = prev_ptr; // next_fit 사용을 위한 포인터 동기화
        //     return prev_ptr;
        // }
        else // 위 케이스에 모두 해당되지 않아, 결국 malloc을 해야 하는 경우
        {
            newptr = mm_malloc(size + DSIZE); // (할당하려는 크기 + 8 bytes)만큼 새롭게 할당
            if (newptr == NULL) // 새로 할당한 주소가 NULL일 경우 (예외처리)
            {
                return NULL;
            }
            memmove(newptr, oldptr, size + DSIZE); // payload 복사
            //lastp = newptr; // next_fit 사용을 위한 포인터 동기화
            mm_free(oldptr); // 기존의 블록은 Free block으로 바꾼다
            return newptr; // 새롭게 할당된 주소의 포인터를 반환
        }
    }
}
*/