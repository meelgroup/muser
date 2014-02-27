/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_fbar.hh
 *
 * Description: Class definition of the GMUS extraction algorithm specialized for
 *              flop-based abstraction-refinement instances.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                               Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_EXTRACTION_ALG_FBAR_HH
#define _MUS_EXTRACTION_ALG_FBAR_HH 1

#include "mus_extraction_alg.hh"


/** This is the implementation of the basic insertion-based MUS extraction
 * algorithm.
 */
class MUSExtractionAlgFBAR : public MUSExtractionAlg {

public:

  MUSExtractionAlgFBAR(IDManager& imgr, ToolConfig& conf, SATChecker& sc,
                       ModelRotator& mr, MUSData& md, GroupScheduler& s)
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s), _solver(sc.solver()) {}

  /* The main extraction logic is implemented here.
   */
  void operator()(void);

public:      // Stats

  /** Returns the number of actual calls to SAT solver
   */
  unsigned sat_calls(void) const { return _sat_calls; }

  /** Returns the total time spent SAT solving only (broken-down per outcome too)
   */
  double sat_time(void) const { return _sat_time; }
  double sat_time_sat(void) const { return _sat_time_sat; }
  double sat_time_unsat(void) const { return _sat_time - _sat_time_sat; }

private:    // Data

  MUSer2::SATSolverWrapper& _solver;// SAT solver to use

private:    // Configuration

  bool _cleanup_online = false;    // if true, cleanup is done on-the-fly

  bool _cleanup_after = true;      // if true, cleanup after the main loop

  bool _skip_witnesses = true;     // if true, do not use witnesses
  
  bool _skip_rcheck = false;       // if true, do not do redundancy check
  
  bool _use_rgraph = false;        // use resolution-graph heuristic for
                                   // selection after SAT outcomes

  bool _set_phase = false;         // if true, set variable phase when using
                                   // res-graph heuristic

  bool _skip_insertion = false;    // if true, insertion phase is skipped

  unsigned _cegar = 0;             // CEGAR type

private:    // Technical

  void _init_data(void);          // initializes internal data

  void _reset_data(void);         // cleans everything up

  bool _untrimmed = false;        // if true the instance was untrimmed

  // main logic

  void _do_cegar(void);           // does "CEGAR" based overapproximation

  void _do_insertion(void);       // does insertion loop

  void _cleanup_cands(GID gid);    // removes all groups made redundant by the 
                                   // addition of gid to cand_gids

  void _cleanup_cands(void);       // removes all redundant groups from cand_gids

  // solving

  IntVector _assumps;             // assumptions for solving

  SATRes _solve(void);            // wrapper for solving

  // working/candidate sets and related stuff
  
  GIDSet _untested_gids;          // untested

  GIDSet _cand_gids;              // candidates for MUS

  GID _pick_next_group(GID last_gid, SATRes last_outcome);  // returns next group to analyze or gid_Undef

  // negation of the group

  void _add_neg_group(GID gid);   // adds negation of the group gid to the solver

  void _remove_neg_group(void);   // undo _add_neg_group()

  IntVector _neg_ass;             // literals used for negation (and to remove it)

  // witnesses

  typedef std::map<GID, IntVector> WMap;        

  WMap _w_map;

  void _store_witness(GID gid, const IntVector& model);

  IntVector& _get_witness(GID gid);

  void _remove_witness(GID gid);

  bool _satisfies_group(GID gid, const IntVector& ass);

  unsigned _num_fclauses(GID gid, const IntVector& ass);

  bool _try_fix_witness(GID cand_gid, IntVector& witness, GID gid);

  bool _is_witness(GID gid, const IntVector& witness);

private:    // Stats

private:   // Testing

  void _check_invariants(void);

};

/*----------------------------------------------------------------------------*/

#endif // _MUS_EXTRACTION_ALG_FBAR_HH
