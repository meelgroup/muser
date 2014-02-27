/*----------------------------------------------------------------------------*\
 * File:        utils.hh
 *
 * Description: Declaration of various utility functions.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#ifndef _UTILS_HH
#define _UTILS_HH 1

#include <cassert>
#include <cstdlib>
#include <sys/time.h>
#include "globals.hh"
#include "basic_clause.hh"
#include "basic_group_set.hh"
#include "cl_types.hh"

namespace Utils {

  /** Returns true if c1 subsumes c2.; relies on the fact the active literals in 
   * clauses are sorted in increasing order of their absolute values.
   */
  bool subsumes(const BasicClause* c1, const BasicClause* c2);

  /** Returns true if the resolvent of the two given clauses on a literal is 
   * tautological; this does the full (long) check; relies on the fact the 
   * active literals in clauses are sorted in increasing order of their 
   * absolute values.
   */
  bool taut_resolvent(const BasicClause *c1, const BasicClause *c2, LINT lit);

  /** Flips a variable in the assignment (assignment is given in terms of -1;0;1
   */
  inline void flip(IntVector& ass, ULINT var) {        
    LINT val = ass[var];
    assert(val != 0); // really shouldn't be flipping unassigned vars
    if (val)
      ass[var] = (val == 1) ? -1 : 1;
  }

  /** Multiflip -- excects a contaner of variables (ULINTS) to flip
   */
  template<class V> inline void multiflip(IntVector& ass, V& vars) {
    for (ULINT var : vars) flip(ass, var);
  }

  /** Returns the truth-value of a literal under assignment: -1;0;1
   */
  inline int tv_lit(const IntVector& ass, LINT lit) {
    int val = ass[abs(lit)];
    return (val > 0) ? ((lit > 0) ? 1 : -1) 
      : ((val < 0) ? ((lit > 0) ? -1 : 1) : 0);
  }

  /** Returns the truth-value of clause under assignment: -1;0:+1
   */
  int tv_clause(const IntVector& ass, const BasicClause* cl);

  /* Returns the number of true literals in the clause under assignment.
   */     
  int num_tl_clause(const IntVector& ass, const BasicClause* cl);
  
  /** Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, 
   * -1 for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
   * is UNSAT iff at least one clause is UNSAT, undetermined otherwise
   */
  int tv_group(const IntVector& ass, const BasicClauseVector& clauses);

  /* Creates a group which represents the CNF of a negation of the clauses in
   * the given clause vector, and adds it as a group 'out_gid' to the group-set
   * 'out-gs'.
   */
  void make_neg_group(const BasicClauseVector& cls,
                      BasicGroupSet& out_gs, GID out_gid, IDManager& imgr);

  /** This is a class that knows to generate the cartesian product of the literals
   * in the given vector of clauses. The usage pattern is: 
   *    call has_next_product(); if true, call next_product() -- the product is a
   *    reference to a vector of literals (owned by the instance).
   * Inappropriate usage is indicated with exceptions.
   */
  class ProductGenerator {

  public:       

    ProductGenerator(const BasicClauseVector& clauses);

    /* Returns true if there's a next product */
    bool has_next_product(void) { return _iters[0] != _clauses[0]->aend(); }

    /* Returns a reference to the next product */
    const vector<LINT>& next_product(void);

  private:

    const BasicClauseVector& _clauses;  // the input clauses

    vector<LINT> _product;              // keeps the next product

    vector<CLiterator> _iters;          // iterators: one per clause

  };

  // random numbers (from crsat)

  /* Initializes random generator. If seed>=0, then the generator is initialized
   * with this value of seed, and the last_rn parameter is ignored. If seed<0,
   * and then if last_rn=0 the current time is used to initialize the generator,
   * otherwise (i.e.e seed<0 and last_rn!=0), the seed is ignored, and the
   * generator is brought to the point as if the last random number generated was
   * last_rn. */
  void init_random(int seed, int last_rn = 0);

  /* Returns a random integer from [0, limit] */
  inline int random_int(int limit) { return (int)((limit+1.0)*rand()/(RAND_MAX+1.0)); }

  /* Returns a random double from [0, 1) */
  inline double random_double() { return ((double)rand()/(RAND_MAX+1.0)); }

  /* Returns the most recently generated random number (without affecting the
   * sequence in any way) */
  int peek_random(void);

  // resolution/conflict graph related utilities

  /* Computes and returns the degree of a group in a resolution graph. The
   * edges within the group itself are ignored.
   */
  unsigned rgraph_degree(const BasicGroupSet& gs, GID gid);

  /* Computes and returns the the sum of the occlists sizes for opposite lits.
   * That is, for clauses, this is a degree of a clause in a conflict graph.
   * For groups, it will be an overestimate, since the function does not
   * detect between the clauses from the same group.
   */
  unsigned cgraph_degree_approx(const BasicGroupSet& gs, GID gid);

}

#endif


