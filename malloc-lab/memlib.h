#include <unistd.h> 
// 유닉스 시스템 호출(sbrk 등)을 사용하기 위한 헤더 파일

void mem_init(void); 
// 가짜 힙을 초기화한다. (malloc 실습용으로 가상 메모리를 설정하는 함수)

void mem_deinit(void); 
// 가짜 힙을 해제한다. (malloc 실습이 끝났을 때 가상 메모리를 free)

void *mem_sbrk(int incr); 
// 현재 가짜 힙을 'incr' 바이트만큼 확장하고, 
// 확장하기 전의 힙 마지막 주소(old break pointer)를 리턴한다
// (진짜 시스템 콜 sbrk를 흉내낸 함수)

void mem_reset_brk(void); 
// 가짜 힙을 초기 상태로 되돌린다. (mem_start_brk로 리셋)
// (= 힙을 통째로 비우는 기능)

void *mem_heap_lo(void); 
// 현재 힙의 '시작 주소'를 리턴한다
// (malloc이 할당할 수 있는 가장 작은 주소)

void *mem_heap_hi(void); 
// 현재 힙의 '가장 마지막 사용 중인 주소'를 리턴한다
// (힙의 끝점, 가장 높은 유효 메모리 주소)

size_t mem_heapsize(void); 
// 현재 가짜 힙의 총 크기를 바이트 단위로 리턴한다
// (mem_heap_hi() - mem_heap_lo() 차이)

size_t mem_pagesize(void); 
// 운영체제의 메모리 페이지 크기(바이트)를 리턴한다
// (보통 4096바이트 = 4KB. 시스템에 따라 다를 수 있음)