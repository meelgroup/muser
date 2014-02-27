/*----------------------------------------------------------------------------*\
 * File:        mus_extraction_alg_prog.hh
 *
 * Description: Class definitions of the progression-based MUS extraction 
 *              algorithms.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                               Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_EXTRACTION_ALG_PROG_HH
#define _MUS_EXTRACTION_ALG_PROG_HH 1

#include "check_range_status.hh"
#include "mus_extraction_alg.hh"
#include "rotate_model.hh"

/** This is the implementation of the progression-based MUS extraction algorithm,
 * together with various methods for analysing the set of target clauses. The
 * default implementation of process_target_clauses() is provided.
 */
class MUSExtractionAlgProg : public MUSExtractionAlg {
  
public:
  
  MUSExtractionAlgProg(IDManager& imgr, ToolConfig& conf, SATChecker& sc, 
                       ModelRotator& mr, MUSData& md, GroupScheduler& s) 
    : MUSExtractionAlg(imgr, conf, sc, mr, md, s), _crs(md), _rm(md) {}

  /* The main extraction logic is implemented here. The implementation sets up
   * the progression loop to build sets of target clauses and then calls 
   * analyze_target_clauses() to handle the set. The pre- and post- conditions
   * of analyze_target_clauses() are listed below.
   */
  void operator()(void);

protected:

  // data

  std::vector<GID> _all_gids;               // vector of groups (ordered by the scheduler)

  std::vector<GID>::iterator _p_removed;    // start of removed; inv: [0,p_removed) \in \UNSAT 
                                            // is the current working formula

  std::vector<GID>::iterator _p_unknown;    // start of unknown; inv: [0,p_unknown) are 
                                            // necessary for [0,p_removed)

  CheckRangeStatus _crs;                    // for SAT solver calls

  RotateModel _rm;                          // for model rotation

  bool _save_model;                         // when true, model is saved on SAT outcomes

  IntVector _last_model;                    // to store and keep the model

  // main funcionality

  /** Analyzes a set of target groups in the interval [p_target,p_removed). As 
   * a result at least one clause is added to the MUS. This is the top-level
   * method called from the main progression loop. The particular analysis 
   * routines are implemented in the atg_...() methods below.
   * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target) is doing MR)
   * @post (p_unknown' > p_unknown) && (p_removed' <= p_removed)
   */
  virtual void analyze_target_groups(vector<GID>::iterator p_target);

  /** Analyzes a set of target groups in the interval [p_target,p_removed) using
   * binary search. All target clauses are considered, unless false_only = true
   * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target) if doinf MR)
   * @post (p_unknown' > p_unknown) && (p_removed' <= p_removed)
   */
  void atg_binary_simple(vector<GID>::iterator p_target, bool false_only = false);

  /** Analyzes a set of target groups in the interval [p_target,p_removed) using
   * simple linear scan. All target clauses are considered, unless false_only = true
   * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target) if doing MR)
   * @post (p_unknown' > p_unknown) && (p_removed' <= p_removed)
   */
  void atg_linear_simple(vector<GID>::iterator p_target, bool false_only = false);

  // utitilies

  /** Prepares the relevant data fields */
  void init_data(void);

  /** Calls the SAT solver to check the status of the range [0, p_range). 
   * @post if SAT and model rotation is on, _last_model will have the model
   * @post if UNSAT and refinement is on, _crs will have unnecessary GIDs
   */
  bool check_range_status(vector<GID>::iterator p_range);

  /** Runs the model rotation algorithm
   * @pre p_nec < p_unknown points to a group necessary for [0,p_removed)
   *      model is its witness
   * @post p_unknown' >= p_unknown
   */
  void do_model_rotation(vector<GID>::iterator p_nec, IntVector& model);

  /** Refines the set [p_from, p_removed) by dropping all groups from the range 
   * that appear in unnec_gids, i.e. all these groups are shifted to past p_removed
   * @param fast - if true, the remaining groups might be reshuffled (faster), 
   *               otherwise the order is preserved.
   * @pre p_from < p_removed
   * @return r = number of groups removed from the range
   * @post p_removed' == p_removed - r
   */
  size_t do_refinement(vector<GID>::iterator p_from, const GIDSet& unnec_gids, bool fast = true);

  /** Shifts falsified groups in the range [p_target, p_removed) to the end of the 
   * range, and adjusts p_target to point to the begining of false clauses.
   * @pre ([0,p_target) \in \SAT) && (_last_model is a model of [0,p_target)
   * @post ([0,p_target') \in \SAT) && (_last_model is a model of [0,p_target') && 
   *       the groups [p_target',p_removed) are false under model
   */
  void shift_false_clauses(vector<GID>::iterator& p_target);

  // stats

  unsigned _dropped_targets_prog = 0;       // number of groups dropped by progression

  unsigned _dropped_targets_search = 0;     // number of groups dropped by search

  unsigned _prog_sat_outcomes = 0;          // number of SAT outcomes in progression

  unsigned _prog_unsat_outcomes = 0;        // number of UNSAT outcomes in progression

  // debugging

  void dump_state(void);

  void check_inv1(void);

  void check_inv2(void);

};



/*----------------------------------------------------------------------------*/

#endif // _MUS_EXTRACTION_ALG_H
