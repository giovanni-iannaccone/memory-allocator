#include <string.h>

#include "../include/allocator.h"

#if defined(__WIN32__) || defined(__WIN64__)
    #include <windows.h>

    #define sbrk(X) fake_sbrk(X)

    void* fake_sbrk(size_t increment) {
        constexpr size_t MAX_HEAP_SIZE = 1024 * 1024;

        static char *heapStart = nullptr;
        static char *currentBreak = nullptr;

        if (heapStart == nullptr) {
            heapStart = (char*)VirtualAlloc(nullptr, MAX_HEAP_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (heapStart == nullptr)
                return (void *)-1;

            currentBreak = heapStart;
        }

        char *newBreak = currentBreak + increment;
        if (newBreak < heapStart || newBreak > heapStart + MAX_HEAP_SIZE)
            return (void *)-1;

        void *oldBreak = currentBreak;
        currentBreak = newBreak;

        return oldBreak;
    }
#else 
    #include <sys/mman.h>
    #include <unistd.h>
#endif

#define FREE            true
#define NOT_FREE        false
#define NO_FREE_BLOCK   nullptr

struct Block {
    size_t size;
    bool free;
    Block *prev;
    Block *next;
    char data[1];
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

static inline size_t align(size_t size) {
    return (size + (sizeof(void*) - 1)) & -sizeof(void*);
}

#if defined(DEBUG) 
    #include <iostream>
    
    void printMemory() {
        int i = 0;
        Block *current = heapStart;

        std::cout << "\n -----[ Heap status ]-----";
        while (current && current <= top) {
            std::cout << "\n [+] Block " << ++i
                  << ":\taddress " << current
                  << "\tsize " << current->size
                  << "\tfree " << current->free;
            current = current->next;
        }
        std::cout << "\n";
        
    }
#endif

static Block* getBlock(void *data) {
    return (Block *)((char *)data - sizeof(Block) + sizeof(data));
}

static bool canSplit(Block *block, size_t size) {
    return block->size >= size + sizeof(Block);
}

static void split(Block *block, size_t size) {
    size_t originalSize = block->size;
    block->size = size;

    Block* newBlock = (Block *)((char *)block + sizeof(Block) + size);
    newBlock->size = originalSize - size;
    newBlock->free = FREE;

    newBlock->next = block->next;
    if (newBlock->next)
        newBlock->next->prev = newBlock;

    block->next = newBlock;
    newBlock->prev = block;
}

static bool canMerge(Block *block) {
    Block* next = block->next;
    return next != nullptr && next <= top && next->free;
}

static void merge(Block *block) {
    Block* next = block->next;

    if (next == nullptr || next->free == NOT_FREE) 
        return;

    block->size += next->size + sizeof(Block);
    block->next = next->next;

    if (block->next)
        block->next->prev = block;   
}

static Block *bestFit(size_t size) {
    Block *best = nullptr;
    for (Block *current = heapStart; current; current = current->next)
        if (current->free && current->size >= size)
            if (!best || current->size < best->size)
                best = current;

    return best;
}

static Block *firstFit(size_t size) {
    for (Block *current = heapStart; current; current = current->next)
        if (current->free && current->size >= size)
            return current;

    return NO_FREE_BLOCK;
}

static Block* nextFit(size_t size) {
    if (searchStart == nullptr)
        searchStart = heapStart;

    Block *current = searchStart;

    do {
        if (current->free && current->size >= size) {
            searchStart = current->next;
            return current;
        }
        current = current->next ? current->next : heapStart;
    } while (current != searchStart);

    return nullptr;
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
    size = align(size + sizeof(Block));
    Block* block = (Block*)sbrk(size);

    if (block == (void*)-1)
        return NO_FREE_BLOCK;

    block->size = size - sizeof(Block);
    block->free = FREE;
    block->next = nullptr;
    block->prev = top;

    if (top)
        top->next = block;

    top = block;

    if (!heapStart)
        heapStart = block;

    return block;
}

void _free(void* data) {
    if (data == nullptr)
        return;

    Block* block = getBlock(data);
    block->free = FREE;

    while (block && canMerge(block)) {
        merge(block);
    }

    Block *prev = block->prev;
    while (prev && prev->free && canMerge(prev)) {
        merge(prev);
        prev = prev->prev;
    }
}

void *_malloc(size_t size) {
    if (size <= 0)
        return nullptr;

    size = align(size);

    Block* block = findBlock(size);

    if (block != NO_FREE_BLOCK) {
        block->free = NOT_FREE;
        return block->data;
    }

    block = requestFromOS(size + sizeof(Block));

    if (block != NO_FREE_BLOCK) {
        block->size = size;
        block->free = NOT_FREE;
        return block->data;
    }

    return nullptr;
}

void *_calloc(size_t n, size_t size) {
    void* ptr = _malloc(n * size);
    if (ptr)
        memset(ptr, 0, n * size);

    return ptr;
}

void *_realloc(void* data, size_t newSize) {
    if (data == nullptr)
        return _malloc(newSize);

    Block* oldBlock = getBlock(data);
    if (newSize <= oldBlock->size)
        return data;

    void* newBlock = _malloc(newSize);
    if (newBlock) {
        memcpy(newBlock, data, oldBlock->size);
        _free(data);
    }

    return newBlock;
}