/*----------------------------------------------------------------------------*\
 * File:        bcp_simplifier.hh
 *
 * Description: Class definition of worker that knows to do BCP-based 
 *              simplifications.
 *
 * Author:      antonb
 * 
 * Notes:
 *      1. IMPORTANT: this implementation is NOT multi-thread safe, and is not
 *      ready for multi-threaded execution.
 *      2. The implementation is heavily influenced by Minisat.
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _BCP_SIMPLIFIER_HH
#define _BCP_SIMPLIFIER_HH 1

#include <ext/hash_map>
#include "simplify_bcp.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  BCPSimplifier
 *
 * Purpose: A worker that knows to do BCP-based simplifications
 *
 * Notes:
 *
 *  1. Currently supported work items: SimplifyBCP
 *
\*----------------------------------------------------------------------------*/

class BCPSimplifier : public Worker {

public:

  // lifecycle

  BCPSimplifier(unsigned id = 0) 
    : Worker(id) {}

  virtual ~BCPSimplifier(void) {}

  // functionality

  using Worker::process;

  /* Handles the SimplifyBCP work item.
   */
  virtual bool process(SimplifyBCP& sb);

  /* Reconstructs the solution (inside sb.md()) in terms of the original
   * clauses. This is only relevant for non-group mode, and will result in
   * the "undoing" of BCP on all necessary clauses. 'sb' is expected to
   * be the instance used during the last call to process()
   */
  void reconstruct_solution(SimplifyBCP& sb);

  // statistics 

protected:


};

#endif /* _BCP_SIMPLIFIER_HH */

/*----------------------------------------------------------------------------*/
