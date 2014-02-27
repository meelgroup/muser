/*----------------------------------------------------------------------------*\
 * File:        mus_extractor_del.hh
 *
 * Description: Class definition of deletion-based MUS extractor.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_EXTRACTOR_DEL_HH
#define _MUS_EXTRACTOR_DEL_HH 1

#include "basic_group_set.hh"
#include "compute_mus.hh"
#include "id_manager.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "sat_checker.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  MUSExtractor
 *
 * Purpose: A worker that has and knows to compute an MUS of a group set.
 *
 * Notes:
 *
 *  1. Currently supported work items: ComputeMUS
 *  2. The implementation is not MT-safe.
 *
\*----------------------------------------------------------------------------*/

class MUSExtractor : public Worker {

public:

  // lifecycle

  /* Warning: IDManager will be used during extraction -- if its shared, make 
   * sure its multithread safe.
   */
  MUSExtractor(IDManager& imgr, ToolConfig& conf, unsigned id = 0)
    : Worker(id), _imgr(imgr), config(conf)
  {}

  virtual ~MUSExtractor(void) {}

  // additional SAT checker -- if set before process(), then the checker will
  // be (one of) the checkers used for extraction; this allows to re-use the
  // clauses learned during pre-processing
  void set_sat_checker(SATChecker* pschecker) { _pschecker = pschecker; }
  SATChecker* sat_checker(void) { return _pschecker; }

  // functionality

  using Worker::process;

  /* Handles the ComputeMUS work item
   */
  virtual bool process(ComputeMUS& cm);

  // extra configuration

  /* Sets the soft limit on elapsed CPU time (seconds). 0 means no limit. */
  void set_cpu_time_limit(double limit) { _cpu_time_limit = limit; }

  /** Sets the limit on the number of iteration, where an "iteration" is typically
   * an iteration of the main loop of the algo. 0 means no limit. */
  void set_iter_limit(unsigned limit) { _iter_limit = limit; }

  // statistics

  /* Returns the elapsed CPU time (seconds) */
  double cpu_time(void) const { return _cpu_time; }

#ifdef MULTI_THREADED
  /* Returns the elapsed wall-clock time (seconds) */
  double wc_time(void) const { return _wc_time; }
#endif

  /* Returns the number of actual calls to SAT solver */
  unsigned sat_calls(void) const { return _sat_calls; }

  /* Returns the number of groups detected using model rotation */
  unsigned rot_groups(void) const { return _rot_groups; }

  /* Returns the number of groups removed with refinement */
  unsigned ref_groups(void) const { return _ref_groups; }

protected:
  
  IDManager& _imgr;             // id manager

  ToolConfig& config;           // configuration (name is good for macros)

  SATChecker* _pschecker = NULL;// pointer to SAT checker (to reuse)

  double _cpu_time_limit = 0;   // soft limit on CPU time 

  unsigned _iter_limit = 0;     // limit on the number of iterations

  double _cpu_time = 0;         // elapsed CPU time (seconds) for extraction

  unsigned _sat_calls = 0;      // number of calls to SAT solver

  unsigned _rot_groups = 0;     // groups detected by model rotation (if enabled)

  unsigned _ref_groups = 0;     // groups removed with refinement

#ifdef MULTI_THREADED
  double _wc_time = 0;          // elapsed wall-clock time (seconds)
#endif
  
};

#endif /* _MUS_EXTRACTOR_DEL_HH */

/*----------------------------------------------------------------------------*/
