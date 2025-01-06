# 💿 Memory allocator 

## 📦 Prerequisites
- Familiarity with C/C++ programming
  
## ⚡ How it works
When a process is executed, the operating system allocates a Virtual Address Space (VAS) for it. This abstraction makes each process believe it has access to the entire memory of the computer, while in reality, it is working with a virtualized view of the physical memory.

The memory of a process is organized into distinct segments, listed below in order of increasing addresses: <br/>
( shown from lower address to higher address )
1. *Text segment* contains executable code
2. *Data segment* stores non-zero initialized static data
3. *Bss segment* holds uninitialized static variables or variables explicitly initialized to zero
4. ***Heap* used for dynamically allocated memory (our focus in this explanation)**
5. *Unmapped area* 
6. *Stack* is used to store function activetion records, local variables and parameters
8. command line arguments, environment variables

### 🔊 Stack and Heap Growth
**Stack**: Grows downward (toward lower addresses). The current stack position is tracked by the stack pointer (`sp`), and increasing the stack size reduces the value of `sp`.
**Heap**: Grows upward (toward higher addresses). Its top is tracked by the program break (`brk`). Increasing the heap size moves `brk` to a higher address.

In high-level programming, we deal with objects, but from the perspective of memory, these are just blocks of raw bytes. The system views these blocks as a series of bits, which can be cast to any data type at runtime.

A fundamental method to increase available memory is by moving the brk. In Linux, this can be done with the `sbrk` system call:
- `sbrk(0)` gives the current brk address
- `sbrk(n)` ncreases brk by n bytes and returns the previous brk
- if sbrk fails, it returns `(void*)-1`
  
Using sbrk, a basic memory allocation function might look like this:
```c++
void *malloc(size_t size) {
  void *p = sbrk(0);
  void *request = sbrk(size);
  if (request == (void*) -1)
    return nullptr; // sbrk failed.

  return p;
}
```
This naive approach raises several issues:
1. how do we free that memory ?
2. how can we recycle a block ?
3. is it efficient to make a system call for every memory request ?

Clearly, a more sophisticated strategy is needed.
To build a better allocator, we need to store metadata about each block of memory alongside the block itself. This metadata is stored in a header and can include:
```c++
struct Block {
  size_t size;              // block's size
  bool free;                // the block is currently free
  Block *prev;              // the block before this
  Block *next;              // the block after this
  void *data;               // a pointer to the first word of user data, aka payload pointer
};
```
### ✔ Memory Alignment
For faster access, a memory block should be **aligned** to it's machine's word size. What does this mean ? Each block should be of a multiple
of 8 bytes on x64 machines or 4 bytes on x32 machines. To align a size to a word's size we can use this function: 

```c++
static inline size_t align(size_t n) {
  return (n + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
}
```
### 👍 Improved Allocation Workflow
Now, we can request new memory from the OS, add values inside the block structure, and return the payload pointer. What we built so far is just 
a sequential allocator; it asks for more and more memory, bumping the brk, and is likely to run out of space eventually. This is not a good
implementation (but still a valid one), so we are going to improve it.

An important point is that we will need to work with headers, so it can be crucial to have a function that returns a block's header:
```c++
static Block* getBlock(void *data) {
    return (Block *)((char *)data - sizeof(Block) + sizeof(data));
}
```

To improve our allocator we need the possibility to free blocks, to achieve this we have to set the used flag to `false`:
```c++
void _free(void *data) {
  auto block = getBlock(data);
  block->used = false;
}
```
A better allocator:
- Maintains a linked list of blocks.
- Searches for free blocks to reuse memory.
- Expands the heap only when necessary using sbrk.

### 🔍 Finding Free Blocks
We need a function to search for a free block that meets the requested size. Common algorithms include:
- *First-fit* returns the first block bigger than the requested size
- *Next-fit* is a variant of the first-fit, it returns the first block bigger than the request size searching from the last successful position on the heap
- *Best-fit* returns the block that has the nearest ( but bigger ) size to the requested one

Each algorithm iterates over the linked list of blocks to find a suitable one. For the algorithms to function, we maintain these pointers:
```c++
static Block *heapStart = nullptr;        // Address of the first block in the heap
static Block *searchStart = nullptr;      // Starting point for searches
static Block *top = nullptr;              // Current top of the heap
```
If no suitable free block is found, the allocator requests more memory from the OS:
```c++
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
```
This function will request memory from the operating system, cast it to `Block` and set field values.
Now our malloc will look like this:
```c++
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
```
### 🧩 Merging 
It can really be useful to merge two free blocks, in order to **iterate faster**, **avoid fragmentation** and **reduce the number of system call**.
```c++
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
```
And when do we merge two blocks ? When we free them, so we have to update our free function 
```c++
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
```
### 🍴 Splitting
We are almost done, what if we have a block of size 64 and we only needed 32 ? Our malloc is going to take all of the 64 block.
It is very important to split big blocks
```c++
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
```
### 🐧 Cross-Platform Compatibility
Now that we've completed our memory allocator, I wanted to make it cross-platform, so I mapped ```sbrk``` to ```VirtualAlloc``` using this macro:
```c++
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
```

## 🌎 Resources 
- Glibc malloc implementation: https://sourceware.org/glibc/wiki/MallocInternals
- Memory allocator: http://dmitrysoshnikov.com/compilers/writing-a-memory-allocator/
- Virtual memory: https://www.youtube.com/watch?v=k0OOmaMwcV4

🐞 Happy low level programming...
