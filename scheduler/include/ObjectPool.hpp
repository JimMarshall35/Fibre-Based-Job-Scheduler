#include <cstddef>
#include <new>
#include <cassert>
#include <array>

template <typename T, std::size_t N>
class ObjectPool {
private:
    struct Slot {
        T object;
        Slot* next;
    };

    
    Slot storage[N];
    Slot* freeList = nullptr;

    Slot* slotAt(std::size_t i) {
        return &storage[i];
    }

public:
    ObjectPool() {
        // build free list
        for (std::size_t i = 0; i < N - 1; i++) {
            slotAt(i)->next = slotAt(i + 1);
        }
        slotAt(N - 1)->next = nullptr;
        freeList = slotAt(0);
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (!freeList) return nullptr; // pool exhausted

        Slot* slot = freeList;
        freeList = freeList->next;

        return &slot->object;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;

        Slot* slot = reinterpret_cast<Slot*>(ptr);

        // push back into free list
        slot->next = freeList;
        freeList = slot;
    }

    ~ObjectPool() {
        // NOTE: does NOT destruct live objects automatically
        // You must ensure everything is freed before destruction
    }
};