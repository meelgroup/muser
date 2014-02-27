/*----------------------------------------------------------------------------*\
 * File:        sat_checker.hh
 *
 * Description: Class definition of SAT checker (worker with a SAT solver).
 *
 * Author:      antonb
 * 
 * Notes:
 *              1. Not MT-safe (and not supposed to be)
 *              2. Designed for MT environments (multiple instances are ok)
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _SAT_CHECKER_HH
#define _SAT_CHECKER_HH

#include "basic_group_set.hh"
#include "check_group_status.hh"
#include "check_group_status_chunk.hh"
#include "check_range_status.hh"
#include "check_subset_status.hh"
#include "check_unsat.hh"
#include "check_vgroup_status.hh"
#include "id_manager.hh"
#include "mus_data.hh"
#include "solver_config.hh"     // NOTE: two versions of this; take from wraps-2/
#include "solver_factory.hh"
#include "solver_wrapper.hh"
#include "trim_group_set.hh"
#include "worker.hh"


//#define STATS 1

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
   * values passed in the SATSolverConfig. Warning: IDManager will be used
   * during solving -- if its shared, make sure its multithread safe.
   */
  SATChecker(IDManager& imgr, SATSolverConfig& config, unsigned id = 0)
    : Worker(id), _imgr(imgr), _sfact(imgr), _config(config), 
      _psolver(&_sfact.instance(config)) {
    // initialize the solver
    _psolver->init_all();
  }

  virtual ~SATChecker(void) {
    _psolver->reset_all();
    _sfact.release();
  }

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

  /* Handles the CheckGroupStatusChunk work item
   */
  virtual bool process(CheckGroupStatusChunk& gsc);

  /* Handles the CheckVGroupStatus work item 
   */
  virtual bool process(CheckVGroupStatus& vgs);

  /* Handles the CheckSubsetStatus work item
   */
  virtual bool process(CheckSubsetStatus& css);

  /* Handles the CheckRangeStatus work item
   */
  virtual bool process(CheckRangeStatus& crs);

  // solver control

  /* Sets the preprocessing mode for the checker:
   * 0 = none, 1 = before the first call only, 2 = always
   */
  void set_pre_mode(int mode) { _pre_mode = mode; }

  /* Returns the reference to the underlying SAT solver
   * NOTE: modifications to the state of the solver will bring the SATChecker 
   * out of sync with the solver, and so most likely will break it. A way to 
   * re-sync them is to remove all groups from the solver, but this will cost.
   * USE AT YOUR OWN RISK
   */
  MUSer2::SATSolverWrapper& solver(void) { return *_psolver; }

  // statistics

  /* Returns the number of actual calls to SAT solver
   */
  unsigned sat_calls(void) const { return _sat_calls; }

  /* Returns the total time spent SAT solving only (broken-down per outcome too)
   */
  double sat_time(void) const { return _sat_time; }
  double sat_time_sat(void) const { return _sat_time_sat; }
  double sat_time_unsat(void) const { return _sat_time - _sat_time_sat; }
  
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
  void vsync_solver(const MUSData& md); // TEMP ?

protected:

  /* Invokes the underlying SAT solver; assumes _psolver->init_run() has been
   * already called
   */
  SATRes solve(const IntVector* assum = nullptr);

  /* In the case the last SAT call returned UNSAT, this method will get the core
   * from the SAT solver, and will add the GIDs of unnecessary groups (i.e. those
   * none of whose clauses are in the core) into unnec_gids; no locking is done
   * in this method, however it reads from md.
   * The third parameter, rr_gid, if not gid_Undef, specifies the gid of the group
   * used for redundancy removal trick -- if the core contains rr_gid, then the
   * refinement cannot be used safely, in this cases unnec_gids stays empty.
   */
  void refine(const MUSData& md, GIDSet& unnec_gids, GID rr_gid = gid_Undef);
  void vrefine(const MUSData& md, GIDSet& unnec_vgids, GIDSet& ft_vgids, GID rr_gid = gid_Undef); // TEMP ?
  
protected:

  IDManager& _imgr;                    // id manager

  MUSer2::SATSolverFactory _sfact;     // SAT solver factory (1-to-1 with solver)

  SATSolverConfig& _config;            // configuration of the SAT solver

  MUSer2::SATSolverWrapper* _psolver;  // SAT solver for this instance

  int _pre_mode = 0;                   // preprocessing mode

  GID2IntMap _aux_map;         // map from group IDs of clauses in the chunk
                               // to auxiliary literals (chunking support)

  GID _aux_long_gid = gid_Undef;// GID of the "long" clause (chunking support)

  unsigned _sat_calls = 0;     // number of calls to SAT solver

  double _sat_time = 0;        // total SAT solving time (for stats)

  double _sat_time_sat = 0;    // total SAT solving time (SAT outcomes)

  double _sat_timer = 0;        // used for timing

  void _start_sat_timer(void) { _sat_timer = RUSAGE::read_cpu_time(); }

  void _stop_sat_timer(SATRes outcome) {
    _sat_timer = RUSAGE::read_cpu_time() - _sat_timer;
    _sat_time += _sat_timer;
    if (outcome == SAT_True)
      _sat_time_sat += _sat_timer;
  }

};

#endif /* _SAT_CHECKER_HH */

/*----------------------------------------------------------------------------*/
