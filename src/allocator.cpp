#include <string.h>
#include <utility>
#include <iostream>

#include "../include/allocator.h"

#if defined(__linux__)
    #include <sys/mman.h>
    #include <unistd.h>
#elif defined(__WIN32__) || defined(__WIN64__)
    #include <stdexcept>
    #include <windows.h>

    #define sbrk(X) fake_sbrk(X)

    void* fake_sbrk(size_t increment) {
        constexpr size_t MAX_HEAP_SIZE = 1024 * 1024;

        static char* heapStart = nullptr;
        static char* currentBreak = nullptr;

        if (heapStart == nullptr) {
            heapStart = (char*)VirtualAlloc(nullptr, MAX_HEAP_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (heapStart == nullptr)
                throw std::runtime_error("Failed to initialize heap using VirtualAlloc");

            currentBreak = heapStart;
        }

        char* newBreak = currentBreak + increment;
        if (newBreak < heapStart || newBreak > heapStart + MAX_HEAP_SIZE)
            return (void*)-1;

        void* oldBreak = currentBreak;
        currentBreak = newBreak;

        return oldBreak;
    }
#endif

#define FREE            true
#define NOT_FREE        false
#define NO_FREE_BLOCK   nullptr

struct Block {
    size_t header;
    void *data;
};

enum class SearchMode {
    BestFit,
    FirstFit,
    NextFit,
};

static Block *heapStart = nullptr;
static Block *searchStart = nullptr;
static Block *top = nullptr;    

static SearchMode searchMode = SearchMode::FirstFit;

static inline size_t align(size_t n) {
    return (n + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
}

static inline size_t allocSize(size_t size) {
  return size + sizeof(Block) - sizeof(std::declval<Block>().data);
}

static inline size_t getBlockSize(Block *block) {
    return block->header & ~1;
}

static inline Block *getNext(Block *block) {
    return (Block*)((char*)block + getBlockSize(block));
}

static inline bool isFree(Block *block) {
    return block->header & 1;
}

#if defined(DEBUG) 

    void printMemory() {
        int i = 0;
        Block *current = heapStart;

        std::cout << "\n -----[ Heap status ]-----";
        while (current && current <= top) {
            std::cout << "\n [+] Block " << ++i
                  << ":\taddress " << current
                  << "\tsize " << getBlockSize(current)
                  << "\tfree " << isFree(current);
            current = getNext(current);
        }
        std::cout << "\n";
        
    }
#endif

static inline void setFree(Block *block, bool free) {
    free
        ? block->header |= 1
        : block->header &= ~1;
}

static inline void setSize(Block *block, size_t size) {
    block->header = size | (block->header & 1);
}

static Block *getBlock(void *data) {
    if (data == nullptr)
        return nullptr;

    return (Block *)((char *)data - sizeof(Block));;
}

static bool canSplit(Block *block, size_t size) {
    return getBlockSize(block) >= size;
}

static void split(Block *block, size_t size) {
    size_t originalSize = getBlockSize(block);
    setSize(block, size);

    Block *next = getNext(block);
    setFree(next, FREE);
    setSize(next, originalSize - size - sizeof(Block));
}

static bool canMerge(Block *block) {
    Block *next = getNext(block);
    return next != nullptr && next <= top && isFree(next);
}

static void merge(Block *block) {
    Block *next = getNext(block);
    size_t nextSize = getBlockSize(next);
    size_t blockSize = getBlockSize(block);

    std::cout << "\n[DEBUG] Merging block at " << block
                  << " (size " << blockSize << ") with next block at " << next
                  << " (size " << nextSize << ")";

    setSize(block, blockSize + nextSize - sizeof(Block));
}

//TODO: implement me
static Block *bestFit(size_t size) {
    return heapStart;
}

static Block *firstFit(size_t size) {
    auto block = heapStart;
    
    while (block && block <= top) {
        if (isFree(block) && getBlockSize(block) >= size)
            return block;
        else
            block = getNext(block);
    }

    return NO_FREE_BLOCK;
}

//TODO: implement me
static Block *nextFit(size_t size) {
    return heapStart;
}

static Block *findBlock(size_t size) {
    Block *block;

    switch (searchMode) {

        case SearchMode::BestFit:
            block = bestFit(size);
            break;

        case SearchMode::FirstFit:
            block = firstFit(size);
            break;

        case SearchMode::NextFit:
            block = nextFit(size);
            break;
    }

    if (block && canSplit(block, size)) 
        split(block, size);
    
    return block;
}

static Block *requestFromOS(size_t size) {
    auto block = (Block *)sbrk(0);

    if (sbrk(size) == (void *)-1)
        return NO_FREE_BLOCK;
    
    setSize(block, size - sizeof(Block));
    setFree(block, FREE);

    if (heapStart == nullptr)
        heapStart = block;
    else
        getNext(top)->data = block;
    
    top = block;
    return block;
}

void _free(void *data) {
    if (data == nullptr) 
        return;

    Block *block = getBlock(data);
    setFree(block, FREE);
    
    std::cout << "\n[DEBUG] Freed block at " << block
              << " with size " << getBlockSize(block) 
              << " and free status " << isFree(block);
              
    if (canMerge(block))
        merge(block);
}

void *_calloc(size_t n, size_t size) {
    void *ptr = _malloc(n * size);
    if (ptr)
        memset(ptr, 0x0, n * size);
    
    return ptr;
}

void *_malloc(size_t size) {
    if (size <= 0)
        return nullptr;

    size = align(size);
    
    Block *block = findBlock(size);

    if (block != NO_FREE_BLOCK) {
        setFree(block, NOT_FREE);
        return block->data;
    }

    block = requestFromOS(size);

    if (block != NO_FREE_BLOCK) {
        setSize(block, size);
        setFree(block, NOT_FREE);

        if (heapStart == nullptr)
            heapStart = block;

        std::cout << block->data << "\n";
        top = block;
        return block->data;
    }

    return nullptr;
}

void *_realloc(void *data, size_t newSize) {
    void *newBlock = _malloc(newSize);
    Block *oldBlock = getBlock(data);

    if (newBlock) {
        memcpy(newBlock, data, getBlockSize(oldBlock));
        _free(data);
    }

    return newBlock;
}