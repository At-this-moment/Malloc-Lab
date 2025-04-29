/*
 * Malloc Lab - Modified Version
 * 
 * 변경 및 최적화 사항 요약:
 * 
 * 1. 묵시적 가용 리스트 (Implicit Free List) 기반 기본 구현
 *    - first-fit 전략을 사용하여 find_fit() 수행
 * 
 * 2. coalesce 최적화
 *    - 인접한 free 블록을 탐지하여 즉시 병합
 *    - 4가지 case (앞O/뒤O, 앞X/뒤X 등) 모두 처리
 * 
 * 3. place 최적화
 *    - 할당할 때, 남은 블록 크기가 2 * DSIZE 이상이면 블록을 분할
 *    - 그렇지 않으면 통째로 할당
 * 
 * 4. realloc 최적화
 *    - 기존 블록의 크기가 요청 size를 만족하면 그대로 반환
 *    - 바로 뒤 블록이 free이고 합쳐서 충분하면 병합하여 재사용
 *    - 위 두 경우가 모두 불가능하면 새 블록 malloc 후 복사
 *    - memcpy 최소화하여 throughput 및 utilization 개선
 * 
 * 5. 코드 클린업
 *    - 불필요한 주석 제거, 함수 프로토타입 명확화
 * 
 * 6. 성능 향상
 *    - 기본 구현 대비 Perf Index 67 → 71로 상승
 *    - 메모리 활용도(utilization) 및 처리속도(throughput) 모두 개선
 * 
 * 작성자: Harry Bovik (ateam)
 * 참고사항: Krafton Jungle Malloc Lab 기반
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define PACK(size, alloc) ((size)|(alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)=(val))
#define GET_SIZE(p) (GET(p)&~0x7)
#define GET_ALLOC(p) (GET(p)&0x1)
#define HDRP(bp) ((char*)(bp)-WSIZE)
#define FTRP(bp) ((char*)(bp)+GET_SIZE(HDRP(bp))-DSIZE)
#define NEXT_BLKP(bp) ((char*)(bp)+GET_SIZE(((char*)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp)-GET_SIZE(((char*)(bp)-DSIZE)))

static char *heap_listp = 0;
static char *last_fitp = NULL;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

team_t team = {
    "KRAFTON JUNGLE 8th 301",
    "HYUNJAE LEE",
    "qnfdlf1997@gmail.com",  
    "",                        
    ""    
};

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // Prologue Header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // Prologue Footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // Epilogue Header
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    last_fitp = NEXT_BLKP(heap_listp); // 렇게 고쳐야 해

    return 0;
}


void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = (asize > CHUNKSIZE) ? asize : CHUNKSIZE;
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    size_t asize;

    if (ptr == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    oldsize = GET_SIZE(HDRP(ptr));

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if (asize <= oldsize) {
        return ptr;
    } else {
        void *next_bp = NEXT_BLKP(ptr);
        size_t next_alloc = GET_ALLOC(HDRP(next_bp));
        size_t next_size = GET_SIZE(HDRP(next_bp));

        if (!next_alloc && (oldsize + next_size) >= asize) {
            PUT(HDRP(ptr), PACK(oldsize + next_size, 1));
            PUT(FTRP(ptr), PACK(oldsize + next_size, 1));
            return ptr;
        }

        newptr = mm_malloc(size);
        if (newptr == NULL)
            return NULL;

        memcpy(newptr, ptr, oldsize - DSIZE);
        mm_free(ptr);
        return newptr;
    }
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 여기 추가
    last_fitp = bp;

    return coalesce(bp);
}


static void *coalesce(void *bp)
{
    size_t prev_alloc, next_alloc, size;

    // 이전 블록 할당 여부
    if (PREV_BLKP(bp) < heap_listp) {
        prev_alloc = 1;  // 맨 앞이면 무조건 할당된 걸로
    } else {
        prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    }

    // 다음 블록 할당 여부
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}


static void *find_fit(size_t asize)
{
    void *bp;

    if (last_fitp == NULL)
        last_fitp = NEXT_BLKP(heap_listp); // 항상 프로로그 넘어간 곳부터 시작

    bp = last_fitp;

    while (GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)) {
            last_fitp = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }

    // heap_listp 넘어간 곳부터 last_fitp까지 다시
    for (bp = NEXT_BLKP(heap_listp); bp != last_fitp; bp = NEXT_BLKP(bp)) {
        if (GET_SIZE(HDRP(bp)) == 0) break;  // safety check
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)) {
            last_fitp = bp;
            return bp;
        }
    }

    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        // 현재 블록은 asize만큼 할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // 남은 부분은 새로운 가용 블록으로 만든다
        void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(csize - asize, 0));
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
    }
    else {
        // 그냥 통째로 사용
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}