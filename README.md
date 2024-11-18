# 💿 Memory allocator 

## 📦 Prerequisites
- c/c++
  
## ⚡ How it works
When we execute a process, our operating system allocates for it a virtual address space ( VAS ). With this method any process believes it has
access to all your computer's memory.

The process memory is divided into: <br/>
( shown from lower address to higher address )
1. *Text segment* contains executable code
2. *Data segment* contains non-zero initialized static data
3. *Bss segment* contains zero-initialized static data and data that will get value at run time
4. ***Heap* contains dynamically allocated date ( we are going to focus on this )**
5. *Unmapped area* 
6. *Stack* is used to store function's activetion records, local variables and parameters
8. command line arguments, environment variables

The stack is free to grow **downward** and it's top (actually the bottom ) is pointed by the sp ( stack pointer ). Increasing the stack size means 
decreasing the sp.

Similarly to the stack, the heap grows **upwards** and it's top is pointed by brk ( program break ). Increasing the heap size means increasing the 
brk.

In high-level languages we deal with **objects**, but from a memory perspective we are just using a **memory block**.
Everything computer knows is that this block is of a certain size and its content is treated as a sequence of bits.
At runtime this block can be casted to be used in our program.

A very basic idea is that increasing brk can give us more memory, there is the sbrk system call to do this. 
- ```sbrk(0)``` gives the current brk address
- ```sbrk(n)``` increases brk by n
- if sbrk fails it will return ```(void*)-1```
  
We can use the sbrk system call to expand the heap and obtain a pointer to new ready-to-use memory, 
```c++
void *malloc(size_t size) {
  void *p = sbrk(0);
  void *request = sbrk(size);
  if (request == (void*) -1)
    return nullptr; // sbrk failed.

  return p;
}
```
but this has several problems:
- how do i free that memory ?
- how can i recycle a block ?
- is it correct to do a system call every time we want some byte ?

Seen this, the simple sbrk implementation is probably not the right path. A better way is to store informations about each block near the block 
itself, the malloc will ask for the requested space plus the header size. The block is represented by this structure:
```c++
struct block {
  size_t size;              // block's size
  bool free;                // the block is currently free
  void *data;               // a pointer to the first word of user data, aka payload pointer
};
```
For faster access, a memory block should be **aligned** to it's machine's word size. What does this mean ? Each block should be of a multiple
of 8 bytes on x64 machines or of 4 bytes on x32 machines. To align a size to a word's size we can use this function: 

```c++
static inline size_t align(size_t n) {
  return (n + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
}
```

Ok, now we can request the new memory from the OS, add values inside the block structure and return the payload pointer. 
What we built is just a sequential allocator, it is asking for more and more memory bumping the brk and is probably going to end it. 
This is not a good implementation ( but still a valid one ) so we are going to modify it. 

A very important point is that we will need to work with headers, so it can be vital to have a function that returns a block's header
```c++
static block *getHeader(void *data) {
  return (block *)((char *)data + sizeof(std::declval<block>().data) - sizeof(block));
}
```

To improve our allocator we need the possibility to free blocks, to achieve this we have to set the used flag to ```false```:
```c++
void _free(void *data) {
  auto block = getHeader(data);
  block->used = false;
}
```
Now we need to reuse blocks, this will reduce the number of system calls as our function will call sbrk only when all heap memory is already in use.
The way to do this is by adding a function that searches for a free block which size is bigger than the requested size. But we will probably find 
more than one block that satisfy our requests, so which is the function going to choose ? We have 3 algorithms for this:
- *First-fit* returns the first block bigger than the requested size
- *Next-fit* is a variant of the first-fit, it returns the first block bigger than the request size searching from the last successful position on the heap
- *Best-fit* returns the block that has the nearest ( but bigger ) size to the requested one
  
For this malloc we are going to use a mix of the 3 algorithms, to obtain the best performances.


## 🌎 Resources 
- Glibc malloc implementation: https://sourceware.org/glibc/wiki/MallocInternals
- Memory allocator: http://dmitrysoshnikov.com/compilers/writing-a-memory-allocator/
- Virtual memory: https://www.youtube.com/watch?v=k0OOmaMwcV4

🐞 Happy low level programming...