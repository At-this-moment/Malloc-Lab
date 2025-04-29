#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE 8
#define DSIZE 16
#define CHUNKSIZE (1<<12)
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

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

int mm_init(void) {
    // printf("[DEBUG] mm_init() 시작\n");

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        // printf("[DEBUG] mem_sbrk(4*WSIZE) 실패\n");
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);

    last_fitp = NEXT_BLKP(heap_listp);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        // printf("[DEBUG] extend_heap 실패\n");
        return -1;
    }

    // printf("[DEBUG] mm_init() 완료\n");
    return 0;
}

void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = (asize > CHUNKSIZE) ? asize : CHUNKSIZE;
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        // printf("extend_heap failed inside mm_malloc\n");
        return NULL;
    }
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size) {
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
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

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

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((bp = mem_sbrk(size)) == (void *)-1) {
        // printf("[DEBUG] mem_sbrk(size=%zu) 실패\n", size);
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 에필로그 블록 재설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    last_fitp = bp;

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = 1;
    if ((char *)bp > heap_listp) {               /* 프로로그 뒤일 때만 */
        prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));  // ← 여기 수정
    }

    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size        = GET_SIZE(HDRP(bp));

    /* 이하 나머지 로직은 그대로 */

    if (prev_alloc && next_alloc) {
        // case 1: 앞, 뒤 모두 할당
        // last_fitp는 bp로 유지
        last_fitp = bp;
        return bp;
    } else if (prev_alloc && !next_alloc) {
        // case 2: 앞은 할당, 뒤는 free
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        last_fitp = bp;
    } else if (!prev_alloc && next_alloc) {
        // case 3: 앞은 free, 뒤는 할당
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        last_fitp = bp;
    } else {
        // case 4: 앞, 뒤 모두 free
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        last_fitp = bp;
    }
    return bp;
}

static void *find_fit(size_t asize) {
    void *bp;

    // last_fitp 초기화 (최초 검색 시 시작점 설정)
    if (last_fitp == NULL)
        last_fitp = NEXT_BLKP(heap_listp);

    // 1. last_fitp부터 힙 끝까지 검색
    bp = last_fitp;
    while (GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)) {
            last_fitp = bp; // 성공 시 포인터 갱신
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }

    // 2. 힙 시작부터 last_fitp까지 재검색 (에필로그 접근 방지)
    for (bp = NEXT_BLKP(heap_listp); 
         GET_SIZE(HDRP(bp)) > 0 && bp != last_fitp;  // 종료 조건: 유효 블록 + last_fitp 미도달
         bp = NEXT_BLKP(bp)) 
    {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)) {
            last_fitp = bp; // 성공 시 포인터 갱신
            return bp;
        }
    }

    // 3. 적합한 블록 없음
    return NULL;
}


static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        void *next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(csize - asize, 0));
        PUT(FTRP(next_bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}