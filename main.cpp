#include <cassert>
#include <memory>
#include <pthread.h>
#include <cstdlib>
#include <stdlib.h>
#include <malloc.h>
#include <iostream>  
#include "ssmem.h"
#include <libvmmalloc.h>

#define NUM_THREADS 8
#define THREAD_WORKLOAD 100000

// per-thread allocators must be declared before including the queue header
__thread ssmem_allocator_t *alloc;
__thread ssmem_allocator_t *volatileAlloc;

#include "queues/OptUnlinkedQ.h"

struct threadArgs {
    OptUnlinkedQ<int> *queue;
    size_t tid;
};

void* threadFunc(void* arg) {
#ifdef DEBUG
	std::cout << "thread has started\n";
#endif
    auto* t_args = static_cast<threadArgs*>(arg);
    int tid = t_args->tid;
    auto* queue = t_args->queue;
    int* dequeued = static_cast<int*>(malloc(sizeof(int)));

    // initialize per-thread allocators as described in ssmem documentation
    alloc = static_cast<ssmem_allocator_t*>(malloc(sizeof(ssmem_allocator_t)));
    if (alloc == nullptr) {
        std::cerr << "malloc failed\n";
        return nullptr;
    }
    ssmem_alloc_init(alloc, SSMEM_DEFAULT_MEM_SIZE, tid);

    volatileAlloc = static_cast<ssmem_allocator_t*>(malloc(sizeof(ssmem_allocator_t)));
    if (volatileAlloc == nullptr) {
        std::cerr << "malloc2 failed\n";
        return nullptr;
    }
    ssmem_alloc_init(volatileAlloc, SSMEM_DEFAULT_MEM_SIZE, tid);

#ifdef DEBUG
    std::cout << "thread " << tid << " has q ptr = " << queue << "\n";
#endif
    assert(alloc && volatileAlloc);

    for (int i = 1; i < THREAD_WORKLOAD; i++) {
        queue->enq(tid * i, tid);
        queue->deq(dequeued, tid);
    }

    return nullptr;
}

int main() {
    // pre-initialize allocators for each thread
	alloc = static_cast<ssmem_allocator_t*>(malloc(sizeof(ssmem_allocator_t)));
    assert(alloc);
    volatileAlloc = static_cast<ssmem_allocator_t*>(malloc(sizeof(ssmem_allocator_t)));
    assert(volatileAlloc);

#ifdef DEBUG
	printf("alloc ptr: %p\nvolatilealloc ptr: %p\n", alloc, volatileAlloc);
#endif

  /*if ssmem_alloc_init throws exceptions and aborts at runtime with error:
	"assert(a->mem != NULL) failed", there isnt enough memory given to the program
  	through export VMMALLOC_SIZE_POOL=<size>, even if assert(alloc/volatileAlloc)
	doesnt fail*/
    ssmem_alloc_init(alloc, SSMEM_DEFAULT_MEM_SIZE, 0);
    ssmem_alloc_init(volatileAlloc, SSMEM_DEFAULT_MEM_SIZE, 0);

    // create queue
    auto* queue = new OptUnlinkedQ<int>();

    pthread_t threads[NUM_THREADS];
    threadArgs tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i].tid = i;
        tids[i].queue = queue;
        pthread_create(&threads[i], nullptr, threadFunc, &tids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    int* dummy = static_cast<int*>(malloc(sizeof(int)));

	// at pairwise tests this if statement should always be true
    if (queue->deq(dummy, 0) == false) {
        std::cout << "empty queue\n";
    }

    return 0;
}

