//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        cl_registry.hh
 *
 * Description: 
 *
 * Author:      jpms
 * 
 * Revision:    $Id$.
 *
 *                                     Copyright (c) 2009, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _CL_REGISTRY_H
#define _CL_REGISTRY_H 1

#include <cmath>
#include <vector>

#include <ext/hash_map>       // Location of STL hash extensions
#include <ext/hash_set>       // Location of STL hash extensions
#include <ext/algorithm>      // Location of STL hash extensions

using namespace std;
using namespace __gnu_cxx;    // Required for STL hash extensions

#include "globals.hh"
#include "basic_clause.hh"
#include "cl_functors.hh"
#include "cl_types.hh"

// if CLRG_CACHE_LITS is defined, the literal vectors are stored and looked up in
// the registry, when new clauses are added; this prevents dublicate clauses, but
// might be expensive in some applications. This is the original behaviour.
#define CLRG_CACHE_LITS

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: ClauseRegistry
 *
 * Purpose: Unique registry for created clauses.
\*----------------------------------------------------------------------------*/
//jpms:ec

class ClauseRegistry {
  friend class BasicClauseSet;
  friend class BasicGroupSet;

public:
  static ClauseRegistry& instance() { return *clreg_instance; }

protected:

  ClauseRegistry() : v2p_map(), c2n_map() { }

  virtual ~ClauseRegistry() {
    /* Clear actual clauses ... */

    v2p_map.clear();
    c2n_map.clear();
  }

public: // open up the interface so that clauses can be created directly

  BasicClause* create_clause(LINT nlits, const LINT lits[]) {
    vector<LINT> clits(lits, lits+nlits);
    sort(clits.begin(), clits.end(), AbsLitLess());
    remove_duplicates(clits);
    BasicClause* ncl = NULL;
#ifdef CLRG_CACHE_LITS
    ncl = lookup_vect(clits);
    if (ncl != NULL) {
      DBG(cout << "Clause already exists: "<<*ncl<<endl;);
      return ncl;
    }
#endif
    ncl = new BasicClause(clits);
    register_clause(ncl);
    return ncl;
  }

  BasicClause* create_clause(vector<LINT>& clits) {
    sort(clits.begin(), clits.end(), AbsLitLess());
    remove_duplicates(clits);
    BasicClause* ncl = NULL;
#ifdef CLRG_CACHE_LITS
    ncl = lookup_vect(clits);
    if (ncl != NULL) {
      NDBG(cout << "Clause already exists: "<<*ncl<<endl;);
      return ncl;
    }
#endif
    ncl = new BasicClause(clits);
    register_clause(ncl);
    return ncl;
  }

  LINT num_cl_refs(BasicClause* cl) {
    c2n_iterator cpos = c2n_map.find(cl);
    assert(cpos != c2n_map.end());
    return cpos->second;
  }

  LINT incr_cl_refs(BasicClause* cl) {
    c2n_iterator cpos = c2n_map.find(cl);
    assert(cpos != c2n_map.end());
    return ++cpos->second;
  }

  LINT decr_cl_refs(BasicClause* cl) {
    c2n_iterator cpos = c2n_map.find(cl);
    assert(cpos != c2n_map.end());
    return --cpos->second;
  }

  void register_clause(BasicClause* ncl) {
#ifdef CLRG_CACHE_LITS
    // Map from lit vect to cl
    vector<LINT>& lits = ncl->cl_lits();
    assert(v2p_map.find(&lits) == v2p_map.end());
    v2p_map.insert(make_pair(&lits, ncl));
#endif
    // Map from cl to ref cnt
    LINT nclref = 0;
    c2n_map.insert(make_pair(ncl, nclref));
  }

  void add_literal(BasicClause* cl, LINT nlit) {
#ifdef CLRG_CACHE_LITS
    CHK(ULINT mapsz = v2p_map.size(););
    vector<LINT>& clits = cl->cl_lits();
    v2p_map.erase(&clits);
#endif
    cl->add_lit(nlit);
#ifdef CLRG_CACHE_LITS
    vector<LINT>& nclits = cl->cl_lits();
    v2p_map.insert(make_pair(&nclits, cl));
    CHK(assert(mapsz == v2p_map.size()));
#endif
  }

  void erase_clause(BasicClause* cl) {
    NDBG(cout << "Erasing clause: " << *cl << endl;);
#ifdef CLRG_CACHE_LITS
    vector<LINT>& clits = cl->cl_lits();
    assert(v2p_map.find(&clits) != v2p_map.end());
    v2p_map.erase(&clits);
#else
    c2n_map.erase(cl); // this should probably be done regardless of CLRG_CACHE_LITS
#endif
    delete cl;
  }

  BasicClause* lookup_vect(vector<LINT>& clits) {
#ifdef CLRG_CACHE_LITS
    assert(is_sorted(clits.begin(), clits.end(), AbsLitLess()));
    iv2cl_iterator ippos = v2p_map.find(&clits);
    return (ippos != v2p_map.end()) ? ippos->second : NULL;
#else
    return NULL;
#endif
  }

  BasicClause* lookup_vect(LINT num, const LINT ivect[]) {
#ifdef CLRG_CACHE_LITS
    vector<LINT> clits(ivect, ivect+num);
    iv2cl_iterator ippos = v2p_map.find(&clits);
    return (ippos != v2p_map.end()) ? ippos->second : NULL;
#else
    return NULL;
#endif
  }

protected:

  // Remove duplicate literals
  void remove_duplicates(vector<LINT>& clits) {
    NDBG(PRINT_ELEMENTS(clits); cout << endl;);
    unsigned int i = 1;
    unsigned int j = 1;
    for(; i<clits.size(); ++i) {
      assert(clits[i] != 0);
      if (clits[i-1] != clits[i]) { clits[j] = clits[i]; ++j; }
      CHK(else { std::cout << "Duplicate lit: " << clits[i] << endl; });
      CHK(if (clits[i-1] == -clits[i]) {
	  std::cout<<"Vacuously SAT cl: ";PRINT_ELEMENTS(clits);cout<<endl;});
    }
    // Resize if new cl size smaller than original; min val of j is 1.
    if (clits.size() > j) { clits.resize(j); }    // Final clause size
    NDBG(PRINT_ELEMENTS(clits, "Final cl: ", " ");)
  }

protected:

  IVec2ClMap v2p_map;

  Clause2IntMap c2n_map;

  static ClauseRegistry* clreg_instance;

};

#endif /* _CL_REGISTRY_H */

/*----------------------------------------------------------------------------*/
