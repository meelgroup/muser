/*----------------------------------------------------------------------------*\
 * File:        occs_list.hh
 *
 * Description: Class definition of occurences list -- currently a map from 
 *              literals to clauses they appear in.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. IMPORTANT: this implementation is NOT multi-thread safe.
 *      2. TODO: important -- provide iterators that automatically drop the
 *         removed clauses. And make an MT-safe version !!!!
 *      3. TODO: possibly lits to groups ? groups to lits ? 
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _OCCS_LIST_HH
#define _OCCS_LIST_HH

#include <ostream>
#include <vector>
#include "basic_clause.hh"
#include "cl_types.hh"
#include "basic_group_set.hh"

/*----------------------------------------------------------------------------*\
 * Class:  OccsList
 *
 * Purpose: A class that maintains various occurences lists.
 *
 * Notes:
 *
 *  1. Currently: map from literals to clauses they appear in. Note that the 
 *  clauses might be marked as deleted -- one key performance-related aspect 
 *  (e.g for BCE) is the ability of knowing the number of actual (i.e. 
 *  non-removed) clauses for each literal without performing a cleanup. 
 *  The _i versions of methods take the *index* of literal, rather than the
 *  literal itself, directly.
 *
\*----------------------------------------------------------------------------*/

class OccsList {

public:         // Mapping ...

  /* Maps literals (positive or negative) to positive integers, that are used 
   * for indexing the map. */
  static int l2i(LINT l) { return (abs(l) << 1) | (l < 0); }
  static LINT i2l(int i) { return (i & 1) ? -(i >> 1) : (i >> 1); }

public:       // Lifecycle

  /* Reserves memory for the map; max_var is the expected maximum variable index. */
  void init(unsigned max_var) { 
    _clauses.reserve(l2i(-(LINT)max_var)+1);
    _active_sizes.reserve(l2i(-(LINT)max_var)+1);
  }

  /* Ensures that the map has support for up to max_var variables. */
  void resize(unsigned max_var) { 
    _clauses.resize(l2i(-(LINT)max_var)+1);
    _active_sizes.resize(l2i(-(LINT)max_var)+1, 0);
  }
  
  /* True if the map is empty */
  bool empty(void) const { return _clauses.empty(); }

  /* Clears everything out */
  void clear(void) { _clauses.clear(); _active_sizes.clear(); }

public:       // Access to clauses

  typedef std::vector<BasicClauseList>::const_iterator cl_citer;

  cl_citer cl_begin(void) const { return _clauses.begin(); }
  cl_citer cl_end(void) const { return _clauses.end(); }

  BasicClauseList& clauses(LINT lit) { return _clauses[l2i(lit)]; }
  const BasicClauseList& clauses(LINT lit) const { return _clauses[l2i(lit)]; }
  // direct
  BasicClauseList& clauses_i(int i) { return _clauses[i]; }
  const BasicClauseList& clauses_i(int i) const { return _clauses[i]; }

public:      // Access to sizes

  typedef std::vector<unsigned>::const_iterator as_citer;

  as_citer as_begin(void) const { return _active_sizes.begin(); }
  as_citer as_end(void) const { return _active_sizes.end(); }

  unsigned& active_size(LINT lit) { return _active_sizes[l2i(lit)]; }
  const unsigned& active_size(LINT lit) const { return _active_sizes[l2i(lit)]; }
  // direct
  unsigned& active_size_i(int i) { return _active_sizes[i]; }
  const unsigned& active_size_i(int i) const { return _active_sizes[i]; }

public:     // Support for operations

  // decrements active size of each literal in the clause
  void update_active_sizes(BasicClause* cl) {
    for (Literator pl = cl->abegin(); pl != cl->aend(); ++pl)
      if (active_size(*pl)) // b/c could be removed
        --active_size(*pl);
  }

public:     // Debugging

  void dump(std::ostream& out = std::cout) const {
    out << "Occurences list: " << std::endl;
    int idx = 0;
    for (OccsList::cl_citer pl = cl_begin(); pl != cl_end(); ++pl, ++idx) {
      if (!pl->empty()) {
        out << "  " << OccsList::i2l(idx) << ":";
        out << "(active size = " << _active_sizes[idx] << "): ";
        for(BasicClauseList::const_iterator pcl = pl->begin(); 
            pcl != pl->end(); ++pcl) {
          cout << " "; (*pcl)->dump(cout);
        }
        cout << std::endl;
      }
    }
  }

  friend std::ostream& operator<<(std::ostream& out, const OccsList& ol) {
    ol.dump(out);
    return out;
  }

private:

  std::vector<BasicClauseList> _clauses;        // index = literal index;
  
  std::vector<unsigned> _active_sizes;            // index = literal index;
};

#endif /* _OCCS_LISTS_HH */

/*----------------------------------------------------------------------------*/
