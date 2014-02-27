#ifndef BOOL_VECTOR_H
#define BOOL_VECTOR_H

#include "mtl/Vec.h"
#include <stdint.h>

namespace Minisat
{

template<class T>
class CBoolVector
{
public:
    CBoolVector() : 
      m_Data(),
      m_nSize(0)
    {
    }

    CBoolVector(uint32_t nSize, bool bInit = false) : 
      m_Data((nSize >> BYTES_SIZE) + 1, (T)bInit),
      m_nSize(nSize)
    {
    }

    inline void Resize(uint32_t nNewSize, bool bInit = false)
    {
        if (nNewSize < m_nSize)
        {
            m_Data.shrink(((m_nSize - nNewSize) >> BYTES_SIZE) + 1);
        }
        else if (nNewSize > m_nSize)
        {
            m_Data.growTo(m_nSize + ((nNewSize - m_nSize) >> BYTES_SIZE) + 1, bInit);
        }
    }

    inline bool Get(uint32_t nInd)
    {
        assert(nInd < m_nSize);
        return (m_Data[nInd >> BYTES_SIZE] >> (nInd & CLEAN_VAL)) & 1;
    }

    inline void Set(uint32_t nInd, bool bVal)
    {
        assert(nInd < m_nSize);
        if (bVal)
        {
            m_Data[nInd >> BYTES_SIZE] |=  1 << (nInd & CLEAN_VAL) ;
        }
        else
        {
            m_Data[nInd >> BYTES_SIZE] &=  ~(1 << (nInd & CLEAN_VAL)) ;
        }
    }

    inline bool operator[] (uint32_t nInd)
    {
        return Get(nInd);
    }

    inline uint32_t CountTrues()
    {
        uint32_t nCount = 0;
        for (int nInd = 0; nInd < m_Data.size(); ++nInd)
        {
            T nCurrSlot = m_Data[nInd];
            if (nCurrSlot == 0)
            {
                continue;   
            }
                
            for (int nShift = 0; nShift < BITS_NUMBER; ++nShift)
            {
                nCount += (nCurrSlot >> nShift) & 1;
            }
        }

        return nCount;
    }

    uint32_t FirstTrue()
    {
        for (int nInd = 0; nInd < m_Data.size(); ++nInd)
        {
            T nCurrSlot = m_Data[nInd];
            if (nCurrSlot > 0)
            {
                for (int nShift = 0; nShift < BITS_NUMBER; ++nShift)
                {
                    if ((nCurrSlot & 1) > 0)
                    {
                        // because type size should be 2^x we could instead of multiply
                        // use shift right which is much faster, but we leave it like that
                        // for a while
                        return nInd * BYTES_SIZE + nShift;
                    }
                } 
            }
        }
    }

private:
    static const uint32_t BYTES_SIZE = sizeof(T);
    static const uint32_t BITS_NUMBER = sizeof(T) << 3;
    static const T CLEAN_VAL = BITS_NUMBER - 1;
    static const T MAX_VAL = (T)(-1);

    vec<T> m_Data;
    uint32_t m_nSize;
};

}

#endif //BIT_VECTOR_H
