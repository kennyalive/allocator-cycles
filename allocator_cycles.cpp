#include <intrin.h>
#include <cstdio>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <vector>

#include "lib/mimalloc.h"

uint64_t get_tsc()
{
    uint32_t aux_dummy;
    uint64_t tsc = (uint64_t)__rdtscp(&aux_dummy);
    return tsc;
}

constexpr bool use_mimalloc = false;

void* allocate(size_t size) {
    if constexpr (use_mimalloc) {
        return mi_malloc(size);
    }
    else {
        return malloc(size);
    }
}

void deallocate(void* ptr) {
    if constexpr (use_mimalloc) {
        mi_free(ptr);
    }
    else {
        free(ptr);
    }
}

void test_fixed_size_allocs(int N)
{
    constexpr int alloc_count = 64;
    void* ptrs[alloc_count];

    uint64_t t0 = get_tsc();
    for (int i = 0; i < alloc_count; i++) {
        ptrs[i] = allocate(N);
    }
    uint64_t t1 = get_tsc();
    
    for (int i = 0; i < alloc_count; i++) {
        deallocate(ptrs[i]);
    }
    uint64_t t = (t1 - t0) / alloc_count;
    printf("t_alloc_size_%d = %" PRIu64 "\n", N, t);
}

void test_varying_alloc_dealloc(int alloc_count)
{
    struct Action {
        bool alloc = false;
        int value = -1; // size for alloc, index for dealloc
    };
    std::vector<Action> actions;

    // generate actions
    {
        const int alloc_size[8] = { 8, 40, 96, 140, 256, 1000, 4096, 20000 };
        actions.reserve(alloc_count * 2); // for each alloc there is a dealloc
        std::vector<int> alloc_indices;
        alloc_indices.reserve(alloc_count); // reserve max possible size
        int next_alloc_index = 0;
        // sequence of initial allocations
        const int initial_alloc_count = alloc_count / 4;
        for (int i = 0; i < initial_alloc_count; i++) {
            int size_index = rand() % 8;
            actions.push_back({ true, alloc_size[size_index] });
            alloc_indices.push_back(next_alloc_index++);
        }
        // mix allocation with deallocation of the previous allocation
        const int alloc_dealloc_count = alloc_count - initial_alloc_count;
        for (int i = 0; i < alloc_dealloc_count; i++) {
            int size_index = rand() % 8;
            actions.push_back({ true, alloc_size[size_index] });
            alloc_indices.push_back(next_alloc_index++);

            int alloc_to_deallocate = rand() % int(alloc_indices.size());
            int alloc_index = alloc_indices[alloc_to_deallocate];
            actions.push_back({ false, alloc_index });
            alloc_indices.erase(alloc_indices.begin() + alloc_to_deallocate);
        }
        // free the rest of allocations
        assert(alloc_indices.size() == initial_alloc_count);
        for (int alloc_index : alloc_indices) {
            actions.push_back({ false, alloc_index });
        }
    }

    // execute actions
    void** ptrs = (void**)allocate(sizeof(void*) * alloc_count);
    memset(ptrs, 0, sizeof(void*)* alloc_count);
    int ptr_count = 0;
    uint64_t t0 = get_tsc();
    for (const auto& action : actions) {
        if (action.alloc) {
            ptrs[ptr_count++] = allocate(action.value);
        }
        else {
            assert(ptrs[action.value] != nullptr);
            deallocate(ptrs[action.value]);
            ptrs[action.value] = nullptr;
        }
    }
    uint64_t t1 = get_tsc();
    deallocate(ptrs);
    uint64_t t = (t1 - t0) / actions.size();
    printf("t_alloc_dealloc_count_%d = %" PRIu64 "\n", alloc_count, t);
}

int main()
{
    test_fixed_size_allocs(8);
    test_fixed_size_allocs(96);
    test_fixed_size_allocs(240);
    test_varying_alloc_dealloc(16);
    test_varying_alloc_dealloc(128);
    test_varying_alloc_dealloc(1000);
    test_varying_alloc_dealloc(10000);
    test_varying_alloc_dealloc(100000);
}
