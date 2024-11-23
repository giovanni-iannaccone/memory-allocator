#ifndef _ALLOCATOR_H_
    #define _ALLOCATOR_H_

    #include <stddef.h>
    #define DEBUG

    enum class SearchMode {
        BestFit,
        FirstFit,
        NextFit,
    };

    void selectAlgorithm(SearchMode algorithm);
    
    void  _free(void* data);
    void *_malloc(size_t size);
    void *_calloc(size_t n, size_t size);
    void *_realloc(void* data, size_t newSize);

    #ifdef DEBUG
        void printMemory();
    #endif

#endif