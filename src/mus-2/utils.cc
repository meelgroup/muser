/*----------------------------------------------------------------------------*\
 * File:        utils.cc
 *
 * Description: Implementation of various utility functions.
 *
 * Author:      antonb
 * 
 *                                               Copyright (c) 2012, Anton Belov
 \*----------------------------------------------------------------------------*/

#include <cstdlib>
#include <sys/times.h>
#include <stdexcept>
#include "globals.hh"
#include "utils.hh"

/* Returns true if c1 subsumes c2.; relies on the fact the active literals in 
 * clauses are sorted in increasing order of their absolute values.
 */
bool Utils::subsumes(const BasicClause* c1, const BasicClause* c2)
{
  assert(!c1->unsorted());
  assert(!c2->unsorted());
  if (c1->asize() >= c2->asize())
    return false;
  // fast check: 1 in abstraction of c1, 0 in absraction of c2 on the same place
  if (c1->abstr() & ~c2->abstr())
    return false;
  // slow check
  register LINT* first = &(const_cast<BasicClause*>(c1)->lits()[0]); 
  register LINT* first_end = first + c1->asize(); 
  register LINT* second = &(const_cast<BasicClause*>(c2)->lits()[0]); 
  register LINT* second_end = second + c2->asize(); 
  while (first != first_end) {
    // find 'first' in the second clause
    while (*second != *first) {
      ++second; 
      if (second == second_end) { return false; }
    }
    ++first;
  }
  return true;
}

/* Returns true if the resolvent of the two given clauses on a literal is 
 * tautological; this does the full (long) check; relies on the fact the 
 * active literals in clauses are sorted in increasing order of their 
 * absolute values.
 */
bool Utils::taut_resolvent(const BasicClause *c1, const BasicClause *c2, LINT lit)
{
  assert(!c1->unsorted());
  assert(!c2->unsorted());
  // merge, implicitly, two sorted regions, look for early termination
  bool res = false; // true when tautological
  CLiterator pl1 = c1->abegin(), pl2 = c2->abegin();
  while (pl1 != c1->aend() && pl2 != c2->aend()) {
    ULINT v1 = abs(*pl1), v2 = abs(*pl2);
    if (v1 < v2)
      ++pl1;
    else if (v1 > v2)
      ++pl2;
    else if (v1 == (ULINT)abs(lit)) { // v1 == v2 == abs(lit)
      ++pl1; ++pl2;
    } else { // v1 == v2
      bool neg1 = *pl1 < 0, neg2 = *pl2 < 0;
      if ((neg1 ^ neg2) == 0) { // same sign
        ++pl1; ++pl2;
      } else { // clashing, but not lit -- tautology
        res = true;
        break;
      }
    }
  }
  return res;
}

/* Returns the truth-value of clause under assignment: -1;0:+1
 */
int Utils::tv_clause(const IntVector& ass, const BasicClause* cl)
{
  unsigned false_count = 0;
  for(CLiterator lpos = cl->abegin(); lpos != cl->aend(); ++lpos) {
    int var = abs(*lpos);
    if (ass[var]) {
      if ((*lpos > 0 && ass[var] == 1) ||
          (*lpos < 0 && ass[var] == -1))
        return 1;
      false_count++;
    }
  }
  return (false_count == cl->asize()) ? -1 : 0;
}

/* Returns the number of true literals in the clause under assignment.
 */     
int Utils::num_tl_clause(const IntVector& ass, const BasicClause* cl)
{
  unsigned res = 0;
  for(CLiterator lpos = cl->abegin(); lpos != cl->aend(); ++lpos) {
    int var = abs(*lpos);
    if (ass[var]) {
      if ((*lpos > 0 && ass[var] == 1) ||
          (*lpos < 0 && ass[var] == -1))
        res++;
    } 
  }
  return res;
}

/* Checks whether a given assignment satisfies a set of clauses; return 1 for SAT, 
 * -1 for UNSAT, 0 for undetermined. A set is SAT iff all clauses are SAT, a set
 * is UNSAT iff at least one clause is UNSAT, undetermined otherwise
 */
int Utils::tv_group(const IntVector& ass, const BasicClauseVector& clauses)
{
  unsigned sat_count = 0;
  for (cvec_citerator pcl = clauses.begin(); pcl != clauses.end(); ++pcl) {
    int tv = tv_clause(ass, *pcl);
    if (tv == -1)
      return -1;
    sat_count += tv;
  }
  return (sat_count == clauses.size()) ? 1 : 0;
}

/* Creates a group which represents the CNF of a negation of the clauses in
 * the given clause vector, and adds it as a group 'out_gid' to the group-set
 * 'out-gs'.
 */
void Utils::make_neg_group(const BasicClauseVector& cls,
                           BasicGroupSet& out_gs, GID out_gid, IDManager& imgr)
{
  DBG(PRINT_PTR_ELEMENTS(cls, "Source clauses: "););
  // optimized version for a singleton group
  if (cls.size() == 1) {
    const BasicClause* src_cl = *(cls.begin());
    // take care of an empty clause: make a fake tautology
    if (src_cl->asize() == 0) {
        vector<LINT> lits;
        lits.push_back(1);
        lits.push_back(-1);
        BasicClause* new_cl = out_gs.create_clause(lits);
        out_gs.set_cl_grp_id(new_cl, out_gid);
    } else {
      for (CLiterator plit = src_cl->abegin(); plit != src_cl->aend(); ++plit) {
        vector<LINT> lits;
        lits.push_back(-*plit);
        BasicClause* new_cl = out_gs.create_clause(lits);
        out_gs.set_cl_grp_id(new_cl, out_gid);
      }
    }
  }
  // generic version
  else {
    assert(cls.size() > 1);
    vector<LINT> long_lits;
    vector<LINT> lits(2);
    for (const BasicClause* cl : cls) {
      ULINT aux_var = imgr.new_id();
      long_lits.push_back(aux_var);
      for_each(cl->abegin(), cl->aend(), [&](LINT lit) {
        lits[0] = -aux_var;
        lits[1] = -lit;
        BasicClause* new_cl = out_gs.create_clause(lits);
        out_gs.set_cl_grp_id(new_cl, out_gid);
      });
    }
    BasicClause* new_cl = out_gs.create_clause(long_lits);
    out_gs.set_cl_grp_id(new_cl, out_gid);
  }
  DBG(cout << "Out group set: " << endl; out_gs.dump(););
}




// implementation of product generator

/* Constructor */
Utils::ProductGenerator::ProductGenerator(const BasicClauseVector& clauses) 
  : _clauses(clauses) 
{
  if (_clauses.empty())
    throw logic_error("Empty vector of clauses passed to "
                      "ProductGenerator::ProductGenerator()");
  for (auto& cl : _clauses)
    _iters.push_back(cl->abegin());
  _product.resize(_clauses.size(), 0);
}

/* Returns a reference to the next product */
const vector<LINT>& Utils::ProductGenerator::next_product(void) 
{ 
  if (!has_next_product())
    throw logic_error("ProductGenerator::next_product() is called when no "
                      "next product is available");
  // populate the current one
  for (size_t i = 0; i < _iters.size(); ++i)
    _product[i] = *_iters[i];
  // advance: starting from the end, increment, if aend(), advance
  for (int i = (int)_iters.size()-1; i >= 0; --i) {
    if (++_iters[i] != _clauses[i]->aend()) // done
      break;
    if (i) // first one can't roll over 
      _iters[i] = _clauses[i]->abegin();
  }
  return _product;
}

// random number generator

namespace {

// this array is used to keep the state of the random generator
int prg_state[2];

}

/* Initializes random generator. If seed>=0, then the generator is initialized
 * with this value of seed, and the last_rn parameter is ignored. If seed<0,
 * and then if last_rn=0 the current time is used to initialize the generator,
 * otherwise (i.e.e seed<0 and last_rn!=0), the seed is ignored, and the
 * generator is brought to the point as if the last random number generated was
 * last_rn. */
void Utils::init_random(int seed, int last_rn)
{
  if ((seed < 0) && (last_rn)) {
    prg_state[1] = last_rn;
    setstate((char*)prg_state);
  } else {
    if (seed < 0) { // make the seed from time ...
        struct timeval tv;
        struct timezone tzp;
        gettimeofday(&tv,&tzp);
        seed = (( tv.tv_sec & 0x000007FF ) * 1000000) + tv.tv_usec;
    }
    initstate((unsigned int)seed, (char*)prg_state, 8);
  }
}

/* Returns the most recently generated random number (without affecting the
 * sequence in any way) */
int Utils::peek_random(void)
{
  return prg_state[1];
}

// resolution/conflict graph related utilities

/* Computes and returns the degree of a group in a resolution graph. The
 * edges within the group itself are ignored.
 */
unsigned Utils::rgraph_degree(const BasicGroupSet& gs, GID gid)
{
  assert(gs.has_occs_list());
  const OccsList& occs = gs.occs_list();
  unsigned res = 0;
  for (const BasicClause* cl : gs.gclauses(gid)) {
    for (auto plit = cl->abegin(); plit != cl->aend(); ++plit) {
      for (const BasicClause* o_cl : occs.clauses(-*plit))
        if (!o_cl->removed() && (o_cl->get_grp_id() != gid) &&
            !Utils::taut_resolvent(cl, o_cl, *plit)) { res++; }
    }
  }
  return res;
}

/* Computes and returns the the sum of the occlists sizes for opposite lits.
 * That is, for clauses, this is a degree of a clause in a conflict graph.
 * For groups, it will be an overestimate, since the function does not
 * detect between the clauses from the same group.
 */
unsigned Utils::cgraph_degree_approx(const BasicGroupSet& gs, GID gid)
{
  assert(gs.has_occs_list());
  const OccsList& occs = gs.occs_list();
  unsigned res = 0;
  for (const BasicClause* cl : gs.gclauses(gid)) {
    for (auto plit = cl->abegin(); plit != cl->aend(); ++plit)
      res += occs.active_size(-*plit);
  }
  return res;
}
