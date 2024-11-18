#include <iostream>
#include "../include/allocator.h"

void firstTest() {
    std::cout << "\n\n\e[0;93mTesting malloc and free\e[0m\n";

    void *ptr1 = _malloc(10);
    void *ptr2 = _malloc(8);
    void *ptr3 = _malloc(4);

    printMemory();

    std::cout << "\nFree Block1 and Block2";
    _free(ptr1);
    _free(ptr2);
    printMemory();

    std::cout << "\nAllocate a block of size Block1 + Block2";
    ptr1 = _malloc(20);
    printMemory();

    _free(ptr1);
    _free(ptr2);
}

void secondTest() {
    std::cout << "\n\n\e[0;93mTesting use of malloc-allocated array\e[0m";

    int *arr = (int*)_malloc(10 * sizeof(int));

    for(int i = 0; i < 10; i++)
        arr[i] = i;

    std::cout << "\nPutting all numbers from 0 to 9 in an array\n";
    for (int i = 0; i < 10; i++)
        std::cout << i << ": " << arr[i] << "\t";
    
    _free(arr);
}

void thirdTest() {
    std::cout << "\n\n\e[0;93mTesting splitting\e[0m";
    
    void *ptr1 = _malloc(10);
    _free(ptr1);

    void *ptr2 = _malloc(8);
    void *ptr3 = _malloc(8);
    printMemory();
}

void forthTest() {
    std::cout << "\n\n\e[0;93mTesting fragmentation\e[0m";

    void *ptr1 = _malloc(16);
    void *ptr2 = _malloc(32);
    void *ptr3 = _malloc(16);

    _free(ptr2);

    void *ptr4 = _malloc(32);

    if (ptr4 != ptr2)
        std::cout << "Block is't correctly recycled\n";

    _free(ptr1);
    _free(ptr3);
    _free(ptr4);
}

int main() {
    std::cout << "Starting test...\n";

    firstTest();
    secondTest();
    thirdTest();
    forthTest();

    return 0;
}