#include "../minisat-hmuc/core/MinimalCore.h"
#include "../minisat-hmuc/utils/ParseUtils.h"
#include "../minisat-hmuc/mtl/Sort.h"
#include "../minisat-hmuc/utils/System.h"

#include <stdio.h>
#include <assert.h>

namespace MinisatHMUC
{

static IntOption  remove_order("MinimalCore", "remove-order", "Which order to remove: 0 - from largest, 1 - smallest, 2 - highest, 3 - lowest\n", 2, IntRange(0, 3));

CMinimalCore::CMinimalCore(SimpSolver& solver): 
      m_bIcInConfl(false)
      , m_Solver(solver)
      , m_nICSize(0)
{}

void CMinimalCore::SetICNum(uint32_t nIcNum)
{
    m_nICSize = nIcNum;
}

void CMinimalCore::PrintData(int unknownSize, int mucSize, int iter, bool last)
{
    printf("c %siter %d time %g not-muc %d unknown %d muc %d\n", 
        last ? "final " : "", 
        iter + 1, 
        cpuTime(),
        m_nICSize - mucSize - unknownSize, 
        unknownSize, 
        mucSize);
}

lbool CMinimalCore::Solve(bool pre)
{
    vec<uint32_t> vecUnknown;
    vec<uint32_t> vecPrevUnknown;
    vec<uint32_t> vecUids;
    vec<Lit> dummy;
    uint32_t nIcForRemove = 0;
    vec<uint32_t> vecUidsToRemove;
    lbool result = l_Undef;
    Set<uint32_t> setMuc;
    Map<uint32_t, uint32_t> mapIcToScore;
//    Set<uint32_t> setNotMuc;
//    m_Solver.nIcNum = m_nICSize;

    // run preprocessing
    double before_time = cpuTime();
    if (!m_bIcInConfl)
        m_bIcInConfl = !m_Solver.eliminate(pre);
    double simplified_time = cpuTime();
    if (m_Solver.verbosity > 0)
    {
        printf("c |  Simplification time:  %12.2f s                                       |\n", simplified_time - before_time);
        printf("c |                                                                             |\n"); 
    }

    int nIteration = 0;
    for (; true; ++nIteration)
    {
        if (!m_bIcInConfl)
            result = ((Solver*)&m_Solver)->solveLimited(dummy);
        else
        {
            result = l_False;
            m_bIcInConfl = false;
            m_Solver.ResetOk();
        }
        if (result == l_False)
        {
            mapIcToScore.clear();
            // First get all the clauses in unsat core
            m_Solver.GetUnsatCore(vecUids);
            printf("c UNSAT core size = %d\n", vecUids.size());
            return l_False;
            //vecUids.removeDuplicated_();
            // for each clause in vecUids check if its ic
            // and mark it as unknown. 
            for (int nInd = 0; nInd < vecUids.size(); ++nInd)
            {
                uint32_t nIc = vecUids[nInd];
                assert(nIc <= m_nICSize);
                if (!setMuc.has(nIc))
                {
                    vecUnknown.push(nIc);
                    if (remove_order < 2)
                    {
                        if (mapIcToScore.has(nIc))
                            ++mapIcToScore[nIc];
                        else
                            mapIcToScore.insert(nIc, 1);
                    }
                }
            }
            vecUnknown.removeDuplicated_();

            PrintData(vecUnknown.size(), setMuc.elems(), nIteration);

            if (vecUnknown.size() == 0)
            {
                break;
            }

            // now lets remove all unused ics and all their clauses
            // for the first iteration all ics are inside so this need
            // a different treatment, for all others we will check 
            // the previous vector

            // build backward resolution relation so it will be much
            // easier to remove cones
            assert(vecUnknown.size() != vecPrevUnknown.size());
            int nIndUnknown = 0;
            int nSize = nIteration == 0 ? m_nICSize : vecPrevUnknown.size();
            vecUidsToRemove.clear();
            for (int nInd = 0; nInd < nSize; ++nInd)
            {
                uint32_t nIcId = nIteration == 0 ? nInd : vecPrevUnknown[nInd];
                if (nIcId != vecUnknown[nIndUnknown])
                {
                    // remove from sat solver
                    // get all the clauses that are related to this ic
                    vecUidsToRemove.push(nIcId);
//                    setNotMuc.insert(nIcId);
                }
                else
                {
                    if (nIndUnknown + 1 < vecUnknown.size())
                    {
                        ++nIndUnknown;
                    }
                }
            }
            // remove their cones
            m_Solver.RemoveClauses(vecUidsToRemove);
        }
        else if (result == l_True)
        {
            if (nIteration == 0)
            {
                // the problem is sat
                return result;
            }

            vecUidsToRemove.clear();
            vecUidsToRemove.push(nIcForRemove);
            m_Solver.BindClauses(vecUidsToRemove);
            // we removed too much ics add the last one back
            setMuc.insert(nIcForRemove);
            remove(vecPrevUnknown, nIcForRemove);

            if (vecPrevUnknown.size() == 0)
            {
                result = l_False;
                break;
            }

            vecPrevUnknown.swap(vecUnknown);
            PrintData(vecUnknown.size(), setMuc.elems(), nIteration);
        }
        else
        {
            // interrupt
            if (nIteration != 0)
                vecPrevUnknown.swap(vecUnknown);
            else
            {
                vecUnknown.growTo(m_nICSize);
                for (uint32_t nInd = 0; nInd < m_nICSize; ++nInd)
                {
                    vecUnknown[nInd] = nInd;
                }
            }

            break;
        }

        switch ((unsigned)remove_order)
        {
        case 0:
            nIcForRemove = GetMaxIc(mapIcToScore);
            mapIcToScore.remove(nIcForRemove);
            break;
        case 1:
            nIcForRemove = GetMinIc(mapIcToScore);
            mapIcToScore.remove(nIcForRemove);
            break;
        case 2:
            nIcForRemove = vecUnknown.last();
            break;
        case 3:
            nIcForRemove = vecUnknown[0];
            break;
        }

        vecUidsToRemove.clear();
        vecUidsToRemove.push(nIcForRemove);
        m_Solver.UpdateFalsifiedClause(nIcForRemove);
        m_Solver.UnbindClauses(vecUidsToRemove);
        vecPrevUnknown.swap(vecUnknown);
        vecUnknown.clear();
    }

    PrintData(vecUnknown.size(), setMuc.elems(), nIteration, true);
    vec<uint32_t> vecMuc;
    setMuc.copyTo(vecMuc);
    sort(vecMuc);

    printf("v ");
    for (int nInd = 0; nInd < vecUnknown.size(); ++nInd)
    {
        printf("%d ", vecUnknown[nInd] + 1);
    }

    for (int nInd = 0; nInd < vecMuc.size(); ++nInd)
    {
        printf("%d ", vecMuc[nInd] + 1);
    }
    printf("0\n");

    return result;
}

uint32_t CMinimalCore::GetMaxIc(Map<uint32_t, uint32_t>& mapIcToScore)
{
    uint32_t maxVal = 0;
    uint32_t maxInd = 0;
    for (int bucketId = 0; bucketId < mapIcToScore.bucket_count(); ++bucketId)
    {
        const vec<Map<uint32_t, uint32_t>::Pair>& bucket = mapIcToScore.bucket(bucketId);
        for (int elId = 0; elId < bucket.size(); ++elId)
        {
           const Map<uint32_t, uint32_t>::Pair& pair = bucket[elId];
           if (pair.data > maxVal)
           {
               maxVal = pair.data;
               maxInd = pair.key;
           }
        }
    }

    return maxInd;
}

uint32_t CMinimalCore::GetMinIc(Map<uint32_t, uint32_t>& mapIcToScore)
{
    uint32_t minVal = UINT32_MAX;
    uint32_t minInd = UINT32_MAX;
    for (int bucketId = 0; bucketId < mapIcToScore.bucket_count(); ++bucketId)
    {
        const vec<Map<uint32_t, uint32_t>::Pair>& bucket = mapIcToScore.bucket(bucketId);
        for (int elId = 0; elId < bucket.size(); ++elId)
        {
           const Map<uint32_t, uint32_t>::Pair& pair = bucket[elId];
           if (pair.data < minVal)
           {
               minVal = pair.data;
               minInd = pair.key;
               if (minVal == 1)
                   return minInd;
           }
        }
    }

    return minInd;
}

}
