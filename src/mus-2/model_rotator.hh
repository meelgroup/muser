/*----------------------------------------------------------------------------*\
 * File:        model_rotator.hh
 *
 * Description: Class definitions of workers that know to do model rotation.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MODEL_ROTATOR_HH
#define _MODEL_ROTATOR_HH 1

#include <ext/hash_map>
#include <unordered_set>
#include <unordered_map>
#include "basic_clset.hh"
#include "basic_group_set.hh"
#include "mus_data.hh"
#include "mus_config.hh"
#include "rotate_model.hh"
#include "solver_wrapper.hh"
#include "types.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  ModelRotator
 *
 * Purpose: An BC for workers that knows to process RotateModel item.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class ModelRotator : public Worker {

public:

  // lifecycle

  ModelRotator(unsigned id = 0) : Worker(id), _num_points(0) {}

  virtual ~ModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm) { return false; }

  // stats

  /* Returns the number of points visited by the rotator */
  ULINT num_points(void) const { return _num_points; }

  /* Prints out the stats */
  virtual void print_stats(std::ostream& out = std::cout) {}

protected:

  ULINT _num_points;
  
};

/*----------------------------------------------------------------------------*\
 * Class:  RecursiveModelRotatorTmpl
 *
 * Purpose: A model rotator that implements the RMR algorith. The template
 *          parameter represents a class whose rotate_through() method will
 *          be called to determine whether or not to rotate through a point.
 *
 * Template requirements:
 *
 *          bool Dec::rotate_through(RotateModel& rm, GID gid, LINT lit)
 *
 * Notes:
 *
 * 1. At the moment, RMR as per FMCAD 2011, and S. Wieringa's addition CP 2012
 * are implememented using this template.
 *
 * 2. TODO: implement ExtendedModelRotator the same way.
 *
\*----------------------------------------------------------------------------*/

template<class Dec>
class RecursiveModelRotatorTmpl : public ModelRotator {

public:

  // lifecycle

  RecursiveModelRotatorTmpl(unsigned id = 0) : ModelRotator(id) {}

  virtual ~RecursiveModelRotatorTmpl(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);

protected:

  Dec _d;         // the decider

};


/*----------------------------------------------------------------------------*\
 * Class:  RecursiveModelRotator
 *
 * Purpose: A model rotator that implements the RMR algorithm (FMCAD-2011)
 *
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *  2. IMPORTANT: the current implementation is not designed for MT environments
 *     (some data is global).
 *
\*----------------------------------------------------------------------------*/

/** A decider for RMR (FMCAD 2011) */
class DeciderRMR {
public:
  /* Decides whether or not to rotate through a group on a specified literal;
   * assumes that group has been already determined necessary. For RMR this
   * is just a check for whether the group is already necessary
   */
  bool rotate_through(RotateModel& rm, GID gid, LINT lit);

  /* Clears internal data structures */
  void clear(void) {}

};
// class itself
typedef RecursiveModelRotatorTmpl<DeciderRMR> RecursiveModelRotator;


/*----------------------------------------------------------------------------*\
 * Class:  SiertModelRotator
 *
 * Purpose: An implementation of model rotator that uses RMR (FMCAD-2011) with
 *          the optimization proposed by Siert Wieringa (CP-12), plus an extra
 *          depth parameter (AICOMM-11)
 * Notes:
 *
\*----------------------------------------------------------------------------*/
/** A decider for Siert's modification to SMR */
class DeciderSMR {

public:
  /* Decides whether or not to rotate through a group on a specified literal;
   * assumes that group has been already determined necessary. For RMR this
   * is just a check for whether the group is already necessary
   */
  bool rotate_through(RotateModel& rm, GID gid, LINT lit);

  /* Clears internal data structures */
  void clear(void) { _gm.clear(); }

private:

  // visited literal count map: index = literal, value = visit count
  typedef std::unordered_map<LINT, unsigned> lit_count_map;
  // visited groups map: index = GID, value = literal count map
  typedef std::unordered_map<GID, lit_count_map> group_map;

  group_map _gm;

  unsigned _depth = 1;

};
// class itself
typedef RecursiveModelRotatorTmpl<DeciderSMR> SiertModelRotator;


/*----------------------------------------------------------------------------*\
 * Class:  ExtendedModelRotator
 *
 * Purpose: A model rotator that implements the EMR algorithm (AIComm'11)
 *
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *  2. IMPORTANT: the current implementation is not designed for MT environments
 *     (some data is global).
 *
\*----------------------------------------------------------------------------*/

class ExtendedModelRotator : public ModelRotator {

public:

  // lifecycle

  ExtendedModelRotator(unsigned id = 0) : ModelRotator(id) {}

  virtual ~ExtendedModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);
  
};

/*----------------------------------------------------------------------------*\
 * Class:  IrrModelRotator
 *
 * Purpose: A model rotator specialized for the general case of the computation 
 *          of irredudant subformulas. In particular, this rotaror knows to 
 *          rotate through satisfying assignments.
 *
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *  2. IMPORTANT: the current implementation is not designed for MT environments
 *     (some data is global).
 *
\*----------------------------------------------------------------------------*/

class IrrModelRotator : public ModelRotator {

public:

  // lifecycle

  IrrModelRotator(unsigned id = 0) : ModelRotator(id) {}

  virtual ~IrrModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);
  
};

/*----------------------------------------------------------------------------*\
 * Class:  VMUSModelRotator
 *
 * Purpose: A model rotator for VMUS computation algorithms.
 *
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *  2. IMPORTANT: the current implementation is not designed for MT environments
 *     (some data is global).
 *
\*----------------------------------------------------------------------------*/

class VMUSModelRotator : public ModelRotator {

public:

  // lifecycle

  VMUSModelRotator(unsigned id = 0) : ModelRotator(id) {}

  virtual ~VMUSModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);
  
};


/*----------------------------------------------------------------------------*\
 * Class:  IntelModelRotator
 *
 * Purpose: This is an experimental model rotator that assumes a certain
 *          structure in the groups and the group 0. Targeted to Intel
 *          A/R instances, and Huan's decomposition instances.
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *  2. Not MT-safe
 *
\*----------------------------------------------------------------------------*/

class IntelModelRotator : public ModelRotator {

public:

  // lifecycle

  IntelModelRotator(ToolConfig& conf, MUSer2::SATSolverWrapper& solver,
                    unsigned id = 0) // config is good for hacking
    : ModelRotator(id), config(conf), _solver(solver) {}

  virtual ~IntelModelRotator(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);

  // convenience typedef to hold variables
  typedef std::unordered_set<ULINT> VarSet;

  // stats

  /** Prints out the stats */
  virtual void print_stats(std::ostream& out = std::cout) {
    out << "c IntelModelRotator stats:" << endl;
    out << "c  targets searched = " << _targets_searched << endl
        << "c  targets found necessary = " << _targets_found_necc << endl
        << "c  targets found unknown = " << _targets_found_unkn << endl
        << "c  targets search time = " << _target_search_time << " sec " << endl
        << "c  targets prove time = " << _target_prove_time << " sec " << endl
        << "c  targets proved = " << _targets_proved  << endl
        << "c  proved targets time = " << _proved_targets_time << " sec " << endl;
  }

protected:

  ToolConfig& config;     // configuration (name is good for macros)

  MUSer2::SATSolverWrapper& _solver;
  
  // parameters for this call (used everywhere)

  MUSData* _pmd = 0;         

  BasicGroupSet* _pgs = 0;

  OccsList* _po_list = 0;
  
  unsigned _max_points = 0;

  bool _use_rgraph = false;

  enum class Fixer { None = 0, SLS, CDCL };
  Fixer _fixer = Fixer::None;

  bool _abort_when_no_target = false;

  bool _allow_revisit_nodes = false;

  unsigned _sls_cutoff = 100000;

  unsigned _sls_tries = 10;

  float _sls_noise = 0.3;

  int _cdcl_max_conf = 10;      // -1 means no maximum

  bool _transmit_model = false; // whether or not transmit the model to solver

  // stats

  unsigned _targets_searched = 0;       // total number of times to search

  unsigned _targets_found_necc = 0;     // number of times found singleton

  unsigned _targets_found_unkn = 0;     // number of times found, but not singleton

  double _target_search_time = 0;       // total target search time

  double _target_prove_time = 0;

  unsigned _targets_proved = 0;

  double _proved_targets_time = 0;

private:

  // Conflict (or resolution) graph analysis routine
  template<class C>
  BasicClause* analyze_graph(const C& fclauses,                // in 
                             const GIDSet& target_gids,        // in
                             bool new_search,                  // in
                             vector<ULINT>* path);             // out

  // Looks for either a new necessary group ID or a new target group ID
  void find_target(GID source_gid,                     // in
                   GIDSet& target_gids,                // in-out
                   HashedClauseSet& new_fclauses,      // in-out
                   GIDSet& new_fgids,                  // in-out
                   IntVector& curr_ass,                // in-out
                   VarSet& multiflip,                  // in-out
                   GID& new_gid,                       // out
                   GID& target_gid);                   // out

  // This is main routine responsible for "fixing" the current assignment
  // using SLS.
  template<class CLSET, class VSET>
  bool fix_assignment_sls(GID target_gid,                // in
                          unsigned cutoff,               // in
                          unsigned tries,                // in
                          float noise,                   // in
                          IntVector& curr_ass,           // in-out
                          VSET& delta,                   // in-out
                          CLSET& init_fclauses);         // in-out

  // This is main routine responsible for "fixing" the current assignment
  // using CDCL.
  template<class CLSET, class VSET>
  bool fix_assignment_cdcl(GID target_gid,               // in
                           unsigned max_conf,            // in
                           IntVector& curr_ass,           // in-out
                           VSET& delta,                   // in-out
                           CLSET& init_fclauses);         // in-out

};


/*----------------------------------------------------------------------------*\
 * Class:  IntelModelRotator2
 *
 * Purpose: This is actually an approximator.
 *
 * Notes:
 *
 *  1. Currently supported work items: RotateModel
 *
\*----------------------------------------------------------------------------*/

class IntelModelRotator2 : public ModelRotator {

public:

  // lifecycle
  IntelModelRotator2(ToolConfig& conf, unsigned id = 0) // config is good for hacking
    : ModelRotator(id), config(conf) {}

  virtual ~IntelModelRotator2(void) {}

  // functionality

  using Worker::process;

  /* Handles the RotateModel work item
   */
  virtual bool process(RotateModel& rm);

protected:

  ToolConfig& config;     // configuration (name is good for macros)

};


#endif /* _MODEL_ROTATOR_HH */

/*----------------------------------------------------------------------------*/
