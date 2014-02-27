/*----------------------------------------------------------------------------*\
 * File:        ve_simplifier.hh
 *
 * Description: Class definition of worker that knows to do VE+subsumtion based
 *              simplifications, aka SatElite-style. Also knows to reconstruct
 *              solutions.
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

#ifndef _VE_SIMPLIFIER_HH
#define _VE_SIMPLIFIER_HH 1

#include "simplify_ve.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  VESimplifier
 *
 * Purpose: A  worker that knows to do VE-based simplifications
 *
 * Notes:
 *
 *  1. Currently supported work items: SimplifyVE
 *
\*----------------------------------------------------------------------------*/

class VESimplifier : public Worker {

public:

  // lifecycle

  VESimplifier(unsigned id = 0) 
    : Worker(id), _unsound(false), _unsound_mr(false) {}

  virtual ~VESimplifier(void) {}

  // functionality

  using Worker::process;

  /* Handles the SimplifyVE work item.
   */
  virtual bool process(SimplifyVE& sv);

  /* Reconstructs the solution (inside sv.md()) in terms of the original
   * clauses. 'sv' is expected to be the instance used during the last 
   * call to process()
   */
  void reconstruct_solution(SimplifyVE& sv);

  /* Returns true if reconstruction is suspected to be unsound */
  bool unsound(void) const { return _unsound; }
  bool unsound_mr(void) const { return _unsound_mr; } // TEMP: multi-resolvents

  // statistics 

protected:

  bool _unsound;            // when true the reconstruction may have been
                            // unsound
  bool _unsound_mr;

};

#endif /* _BCP_SIMPLIFIER_HH */

/*----------------------------------------------------------------------------*/
