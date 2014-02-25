/*----------------------------------------------------------------------------*\
 * File:        mus_extractor_del.hh
 *
 * Description: Class definition of MUS extractor.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_EXTRACTOR_HH
#define _MUS_EXTRACTOR_HH 1

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

  MUSExtractor(IDManager& imgr, ToolConfig& conf, unsigned id = 0)
    : Worker(id), _imgr(imgr), config(conf), _pschecker(NULL),
      _cpu_time(0), _sat_calls(0), _rot_groups(0), _ref_groups(0)
  {}

  virtual ~MUSExtractor(void) {}

  // additional SAT checker -- if set before process(), then the checker will
  // be used for extraction; this allows to re-use the clauses learned before
  // extraction
  void set_sat_checker(SATChecker* pschecker) { _pschecker = pschecker; }
  SATChecker* sat_checker(void) { return _pschecker; }

  // functionality

  using Worker::process;

  /* Handles the ComputeMUS work item
   */
  virtual bool process(ComputeMUS& cm);

  // statistics

  /* Returns the elapsed CPU time (seconds) */
  double cpu_time(void) const { return _cpu_time; }

  /* Returns the number of actual calls to SAT solver */
  unsigned sat_calls(void) const { return _sat_calls; }

  /* Returns the number of groups detected using model rotation */
  unsigned rot_groups(void) const { return _rot_groups; }

  /* Returns the number of groups removed with refinement */
  unsigned ref_groups(void) const { return _ref_groups; }

protected:
  
  IDManager& _imgr;             // id manager

  ToolConfig& config;           // configuration (name is good for macros)

  SATChecker* _pschecker;       // pointer to SAT checker (to reuse)

  double _cpu_time;             // elapsed CPU time (seconds) for extraction

  unsigned _sat_calls;          // number of calls to SAT solver

  unsigned _rot_groups;         // groups detected by model rotation (if enabled)
  
  unsigned _ref_groups;         // groups removed with refinement

};

#endif /* _MUS_EXTRACTOR_DEL_HH */

/*----------------------------------------------------------------------------*/
