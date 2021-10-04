/**
 * Here we are implementing the Cheney's algorithm as explained in
 * https://en.wikipedia.org/wiki/Cheney%27s_algorithm.
 *
 */

#include "balrt.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <sys/mman.h>

#define HEAP_NOT_ENOUGH 7

static uint64_t DEFAULT_HEAP_HALF_SIZE = 3221225473; // 3GB
static uint64_t ROOT_HEADER_SIZE = 8;                // in bytes

// TODO: Try to reduce these global vars
uint8_t *from_space_ptr;
uint8_t *to_space_ptr;
uint8_t *alloc_ptr;
uint8_t *scan_ptr;
uint8_t *heap_limit_ptr;

uint64_t heap_half_size;

typedef uint8_t *Root; // Denotes address of the heap
typedef void (*mark_roots)(Root *, Root);
extern void get_roots(mark_roots, uint8_t*);

//TODO: Add static methods

void _bal_init_heap() {
    int page_size = getpagesize();
    const char *heap_env_val = getenv("BAL_HEAP");
    // TODO: Abort if atol returns seg fault
    heap_half_size = (heap_env_val != NULL) ? atol(heap_env_val) : DEFAULT_HEAP_HALF_SIZE;
    // Make sure heap size is a multiple of page size
    if (heap_half_size % page_size != 0) {
        heap_half_size = ((heap_half_size / page_size) + 1) * page_size;
    }

    // This is done only for testing purposes
    const char *small_heap = getenv("SMALL_HEAP");
    if (getenv("SMALL_HEAP")) {
        heap_half_size = atoi(small_heap);
        if (heap_half_size % 8 != 0) {
            printf("Small heap size should be a multiple of 8.\n");
            abort();
        }
        // printf("Heap half size : %d", heap_half_size);
    }

    // TODO: Removing MAP_ANONYMOUS fails the mmap(), check this
    // TODO: Use PROT_NONE
    from_space_ptr = mmap(NULL, heap_half_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    // TODO: check order and alignemt.
    if (from_space_ptr == MAP_FAILED) {
        printf("Heap initialization Failed\n");
        abort();
    }
    alloc_ptr = from_space_ptr;
    heap_limit_ptr = from_space_ptr + heap_half_size;

    to_space_ptr = mmap(NULL, heap_half_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    // printf("to_space_ptr : %p\n", to_space_ptr);
    // TODO: check order and alignemt.
    if (to_space_ptr == MAP_FAILED) {
        printf("Heap initialization Failed\n");
        abort();
    }
}

// the argument should be the address of stack which points to a location on heap.
// TODO: check to reduce the number of args to one by extracting the ptr from tag,
// inside this method.
void copy(Root *root_ptr, Root root) {
    // printf("COPY\n");
    if (root == NULL) {
        root = *root_ptr;
    }  
    // printf("copy root : %p\n", *root_ptr);
    Root old_root = root - ROOT_HEADER_SIZE;
    uint64_t *old_root_header_ptr = (uint64_t *)old_root;
    uint64_t old_root_header = *old_root_header_ptr;
    // TODO: present this check in more robust way
    if (!(old_root_header & 1)) { // last bit is 0, no forward pointer
        Root new_root = alloc_ptr;
        old_root_header = old_root_header + ROOT_HEADER_SIZE;
        alloc_ptr = alloc_ptr + old_root_header; // heap header contains the size of object,
                                               // alloc_ptr points to next new root
        // printf("root : %p\n", old_root);
        memcpy(new_root, old_root, old_root_header);
        *old_root_header_ptr = (uint64_t)(new_root + ROOT_HEADER_SIZE) | 0x1; // set header and mark it as forward pointer
    }
    *root_ptr = (Root)(*old_root_header_ptr ^ 1);
}

void collect(uint8_t* rsp) {
    mprotect(to_space_ptr, heap_half_size, PROT_READ | PROT_WRITE);
    alloc_ptr = to_space_ptr;
    scan_ptr = to_space_ptr;

    // printf("before get_roots\n");
    get_roots(copy, rsp);
    // printf("afrer get_roots\n");

    while (scan_ptr < alloc_ptr) {
        uint64_t root_size = *(uint64_t *)scan_ptr;
        // printf("root size : %ld\n", root_size);
        scan_ptr = scan_ptr + ROOT_HEADER_SIZE;
        Root root = scan_ptr;

        // Iterate over each multiple of 8 bytes and extract the tag.
        // If tag is zero, it can be a raw pointer or just an integer.
        // So we have to check whether that value available within 
        // the from-space addresses.

        // TODO: Check why we need to check for each root_size / 8 times
        // printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
        for (size_t i = 0; i < root_size / 8; i++) {
            Root *root_ptr = (Root *)(scan_ptr + i * 8);
            // printf("Root ptr : %p\n", root_ptr);
            uint64_t root = *(uint64_t *)root_ptr;
            int tag = getTag((TaggedPtr)root);
            switch (tag & UT_MASK) {
                case 0: // Raw pointer or integer value
                    if (root <= heap_limit_ptr && root >= from_space_ptr) {
                        copy(root_ptr, NULL);
                        // printf("0\n");
                    }
                    break;
                case TAG_INT:
                        // printf("1\n");
                    // TODO: handle if integer is heap allocated
                    break;
                case TAG_LIST_RW:
                        // printf("2\n");
                    copy(root_ptr, (Root) taggedToPtr((TaggedPtr) root));
                    *root_ptr = (Root)ptrAddShiftedTag((UntypedPtr)*root_ptr, ((uint64_t)tag) << TAG_SHIFT);
                    break;
                case TAG_XML_RW:
                        // printf("3\n");
                    fprintf(stderr, "TAG_XML_RW");
                    abort();
                default:
                        // printf("4\n");
                    fprintf(stderr, "1 unknown tag %d\n", tag);
                    abort();
            }
        }

        scan_ptr = scan_ptr + root_size;
    }
    // Fill the from_space from 0 (not necessary)
    memset(from_space_ptr, 0, heap_half_size);

    // swap from_space <-> to_space
    uint8_t* t = from_space_ptr;
    from_space_ptr = to_space_ptr;
    to_space_ptr = t;

    // update end of the heap
    heap_limit_ptr = from_space_ptr + heap_half_size;
    mprotect(to_space_ptr, heap_half_size, PROT_NONE);
}

int tot_bytes = 0;
UntypedPtr _bal_alloc(uint64_t nBytes) {
    // TODO: Replace this with existing method
    // TODO: check whether this overflow
    if (nBytes % 8 != 0) {
        uint64_t x = roundUpUint64(nBytes);
        nBytes = ((nBytes / 8) + 1) * nBytes;
        assert(x == nBytes);
    }
    nBytes = nBytes + ROOT_HEADER_SIZE;
    // printf("alloc_ptr : %p\n", alloc_ptr);
    tot_bytes = tot_bytes + nBytes;
    // printf("nBytes : %d, tot_bytes : %d\n", nBytes, tot_bytes);
    if (alloc_ptr + nBytes > heap_limit_ptr) {
        // printf("Before collect\n");
        collect(__builtin_frame_address(0) + 16);
        // printf("After collect\n");
    }
    if (alloc_ptr + nBytes > heap_limit_ptr) {
        fprintf(stderr, "%s\n", "heap is not enough");
        abort();
    }
    *((uint64_t *)alloc_ptr) = nBytes - ROOT_HEADER_SIZE; // header contains the size of object with out header size
    UntypedPtr p = (UntypedPtr)(alloc_ptr + ROOT_HEADER_SIZE);
    alloc_ptr = alloc_ptr + nBytes;
    // printf("p : %p\n", p);
    return p;
}
