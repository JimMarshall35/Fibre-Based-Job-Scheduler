#pragma once
#include <vector>
#include <memory>
#include <atomic>

typedef size_t Handle;

/*
    designed for use with c style objects, simple allocations of memory.
*/
template<typename T>
class ThreadsafeObjectPool {
public:
    ThreadsafeObjectPool(int size)
        :m_pObjects(std::make_unique<T[]>(size)),
        m_pFreeIndices(std::make_unique<size_t[]>(size)),
        m_nSize(size)
    {
        for(int i=0; i<size; i++)
        {
            m_pFreeIndices[i] = i;
        }
        m_nNextIndex.store(0);
    }
    Handle Allocate()
    {
        Handle n = m_nNextIndex.fetch_add(1);
        assert((n <= m_nSize - 1 ) && "Can't allocate ThreadsafeObjectPool");
        return m_pFreeIndices[n];
    }
    void Deallocate(Handle h)
    {
        
        // Handle val = m_nNextIndex.load();   
        // do
        // {
        //     Handle valDec = val - 1;
        //     m_pFreeIndices[valDec] = h; /* this is wrong, isn't it*/
        // } while (m_nNextIndex.compare_exchange_weak(val, valDec));
        
    }
private:
    std::unique_ptr<T[]> m_pObjects = nullptr;
    std::unique_ptr<Handle[]> m_pFreeIndices = nullptr;
    std::atomic<Handle> m_nNextIndex = 0;
    size_t m_nSize;
};

