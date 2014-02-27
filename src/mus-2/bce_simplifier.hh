/*----------------------------------------------------------------------------*\
 * File:        bce_simplifier.hh
 *
 * Description: Class definition of worker that knows to do blocked clauses
 *              elimination.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. IMPORTANT: this implementation is NOT multi-thread safe.
 *      2. Current implementation supports destructive processing only (i.e. 
 *      use only during pre-processing).
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _BCE_SIMPLIFIER_HH
#define _BCE_SIMPLIFIER_HH 1

#include <ext/hash_map>
#include "basic_clset.hh"
#include "simplify_bce.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  BCESimplifier
 *
 * Purpose: A worker that knows to do BCE-based simplifications
 *
 * Notes:
 *
 *  1. Currently supported work items: SimplifyBCE
 *
\*----------------------------------------------------------------------------*/

class BCESimplifier : public Worker {

public:

  // lifecycle

  BCESimplifier(unsigned id = 0) 
    : Worker(id) {}

  virtual ~BCESimplifier(void) {}

  // functionality

  using Worker::process;

  /** Handles the SimplifyBCE work item.
   * Note: only desctructive mode is supported for now; this will write-lock 
   * the MUSData inside 'sb' for the duration of the call.
   */
  virtual bool process(SimplifyBCE& sb);

  /** This is for external callers: runs BCE on the specified clause-set. The
   * eliminated clauses will be removed from the clause set. Note that it will
   * create its own OccsList for this.
   */
  void simplify(BasicClauseSet& clset);

protected:

  /* This is the actual elimination logic; works on the OccsList; if psb != nullptr,
   * updates it accordingly; if pclset != nullptr detaches clauses from it
   */
  void simplify(OccsList& o_list, SimplifyBCE* psb, BasicClauseSet* pclset);

};

#endif /* _BCE_SIMPLIFIER_HH */

/*----------------------------------------------------------------------------*/
