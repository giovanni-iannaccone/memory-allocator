#ifndef _ALLOCATOR_H_
    #define _ALLOCATOR_H_

    #include <stddef.h>
    #define DEBUG

    void *_malloc(size_t size);
    void *_calloc(size_t n, size_t size);
    void *_realloc(void* data, size_t newSize);
    void  _free(void* data);

    #ifdef DEBUG
        void printMemory();
    #endif

#endif