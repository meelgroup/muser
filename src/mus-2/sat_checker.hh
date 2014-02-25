/*----------------------------------------------------------------------------*\
 * File:        sat_checker.hh
 *
 * Description: Class definition of SAT checker (worker with a SAT solver).
 *
 * Author:      antonb
 * 
 * Notes:
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _SAT_CHECKER_HH
#define _SAT_CHECKER_HH

#include "basic_group_set.hh"
#include "check_group_status.hh"
#include "check_range_status.hh"
#include "check_unsat.hh"
#include "id_manager.hh"
#include "mus_data.hh"
#include "solver_config.hh"     // NOTE: two versions of this; take from wraps-2/
#include "solver_factory.hh"
#include "solver_wrapper.hh"
#include "trim_group_set.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  SATChecker
 *
 * Purpose: A worker that has and knows to run a SAT solver.
 *
 * Notes:
 *
 *  1. Currently supported work items: CheckGroupStatus, TrimGroupSet, 
 *     CheckUnsat
 *  2. Designed for MT environments (i.e. multiple instances are ok)
 *
\*----------------------------------------------------------------------------*/

class SATChecker : public Worker {

public:

  // lifecycle

  /* Constructor makes an underlying instance of SAT solver based on the
   * values passed in the SATSolverConfig.
   */
  SATChecker(IDManager& imgr, SATSolverConfig& config, unsigned id = 0);

  virtual ~SATChecker(void);

  // functionality

  using Worker::process;

  /* Handles the CheckGroupStatus work item by running a SAT check on the
   * appropriate instance
   */
  virtual bool process(CheckGroupStatus& gs);

  /* Handles the TrimGroupSet work item
   */
  virtual bool process(TrimGroupSet& tg);

  /* Handles the CheckUnsat work item
   */
  virtual bool process(CheckUnsat& cu);

  /* Handles the CheckRangeStatus work item
   */
  virtual bool process(CheckRangeStatus& crs);

  /* Returns the reference to the underlying SAT solver
   * NOTE: modifications to the state of the solver will bring the SATChecker 
   * out of sync with the solver, and so most likely will break it. A way to 
   * re-sync them is to remove all groups from the solver, but this will cost.
   * USE AT YOUR OWN RISK
   */
  SATSolverWrapper& solver(void) { return *_psolver; }

  // statistics

  /* Returns the number of actual calls to SAT solver
   */
  unsigned sat_calls(void) const { return _sat_calls; }

  /* Returns the total time spent SAT solving only
   */
  double sat_time(void) const { return _sat_time; }

public:

  /* Loads the groupset into the SAT solver. This methods expects that the SAT
   * solver is empty. The removed groups will not be added, and the final groups
   * will be finalized. Suitable for deletion-based algorithms.
   */
  void load_groupset(const MUSData& md);

  /* Synchronizes the SAT solver with the current state of MUS data; no locking
   * is done in this methods
   * Notes:
   *   see worker.cc for important notes on this method (TODO: move here)
   */
  void sync_solver(const MUSData& md);

protected:

  /* In the case the last SAT call returned UNSAT, this method will get the core
   * from the SAT solver, and will add the GIDs of unnecessary groups (i.e. those
   * none of whose clauses are in the core) into unnec_gids; reads from md.
   * The third parameter, rr_gid, if not gid_Undef, specifies the gid of the group
   * used for redundancy removal trick -- if the core contains rr_gid, then the
   * refinement cannot be used safely, in this cases unnec_gids stays empty.
   */
  void refine(const MUSData& md, GIDSet& unnec_gids, GID rr_gid = gid_Undef);

protected:

  IDManager& _imgr;                  // id manager

  SATSolverFactory _sfact;           // SAT solver factory (1-to-1 with solver)

  SATSolverConfig& _config;          // configuration of the SAT solver

  SATSolverWrapper* _psolver;        // SAT solver for this instance

private:

  unsigned _sat_calls;               // number of calls to SAT solver

  double _sat_time;                  // SAT solving time (for stats)

  GID2IntMap _aux_map;              // map from group IDs of clauses in the chunk
                                    // to auxiliary literals (chunking support)

  GID _aux_long_gid;                // GID of the "long" clause (chunking support)

};

#endif /* _SAT_CHECKER_HH */

/*----------------------------------------------------------------------------*/
