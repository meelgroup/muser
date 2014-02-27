/*******************************************************************************************[Map.h]
Copyright (c) 2006-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef Minisat_Set_h
#define Minisat_Set_h

#include "mtl/IntTypes.h"
#include "mtl/Vec.h"
#include "mtl/Map.h"

namespace Minisat {

//=================================================================================================
// Hash table implementation of Maps
//

template<class K, class H = Hash<K>, class E = Equal<K> >
class Set {
 public:

 private:
    H          hash;
    E          equals;

    vec<K>*    table;
    int        cap;
    int        size;

    // Don't allow copying (error prone):
    Set<K,H,E>&  operator = (Set<K,H,E>& other) { assert(0); }
                   Set        (Set<K,H,E>& other) { assert(0); }

    bool    checkCap(int new_size) const { return new_size > cap; }

    int32_t index  (const K& k) const { return hash(k) % cap; }
    void   _insert (const K& k) { 
        vec<K>& ps = table[index(k)];
        ps.push(k); 
    }

    void    rehash () {
        const vec<K>* old = table;

        int old_cap = cap;
        int newsize = primes[0];
        for (int i = 1; newsize <= cap && i < nprimes; i++)
           newsize = primes[i];

        table = new vec<K>[newsize];
        cap   = newsize;

        for (int i = 0; i < old_cap; i++){
            for (int j = 0; j < old[i].size(); j++){
                _insert(old[i][j]); }}

        delete [] old;

        // printf(" --- rehashing, old-cap=%d, new-cap=%d\n", cap, newsize);
    }

    
 public:

    Set () : table(NULL), cap(0), size(0) {}
    Set (const H& h, const E& e) : hash(h), equals(e), table(NULL), cap(0), size(0){}
    ~Set () { delete [] table; }

    bool insert(const K& k) 
    { 
        if (has(k))
            return false;
        if (checkCap(size+1)) 
            rehash(); 
        _insert(k); 
        size++; 
        return true;
    }

    bool has (const K& k) const {
        if (size == 0) return false;
        const vec<K>& ps = table[index(k)];
        for (int i = 0; i < ps.size(); i++)
            if (equals(ps[i], k))
                return true;
        return false;
    }

    void add(const vec<K>& from)
    {
        for (int i = 0; i < from.size(); ++i)
        {
            insert(from[i]);
        }
    }

    void copyTo(vec<K>& to)
    {
        for (int i = 0; i < cap; i++){
            for (int j = 0; j < table[i].size(); j++){
                to.push(table[i][j]); }}
    }

    // PRECONDITION: the key must exist in the map.
    void remove(const K& k) {
        assert(table != NULL);
        vec<K>& ps = table[index(k)];
        int j = 0;
        for (; j < ps.size() && !equals(ps[j], k); j++);
        assert(j < ps.size());
        if (j >= ps.size())
            return;
        ps[j] = ps.last();
        ps.pop();
        size--;
    }

    void clear () {
        cap = size = 0;
        delete [] table;
        table = NULL;
    }

    int  elems() const { return size; }
    int  bucket_count() const { return cap; }

    // NOTE: the hash and equality objects are not moved by this method:
    void moveTo(Set& other){
        delete [] other.table;

        other.table = table;
        other.cap   = cap;
        other.size  = size;

        table = NULL;
        size = cap = 0;
    }

    // NOTE: given a bit more time, I could make a more C++-style iterator out of this:
    const vec<K>& bucket(int i) const { return table[i]; }
};

//=================================================================================================
}

#endif
