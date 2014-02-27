/*----------------------------------------------------------------------------*\
 * File:        tester.hh
 *
 * Description: Class definition of the correctness tester.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_TESTER_HH
#define _MUS_TESTER_HH 1

#include "basic_group_set.hh"
#include "id_manager.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "sat_checker.hh"
#include "test_irr.hh"
#include "test_mus.hh"
#include "test_vmus.hh"
#include "worker.hh"

/*----------------------------------------------------------------------------*\
 * Class:  Tester
 *
 * Purpose: A worker that knows to test results of MUS or other computations.
 *
 * Notes:
 *  1. The implementation is not MT-safe.
 *
\*----------------------------------------------------------------------------*/

class Tester : public Worker {

public:

  // lifecycle

  Tester(IDManager& imgr, ToolConfig& conf, unsigned id = 0)
    : Worker(id), _imgr(imgr), config(conf)
  {}

  virtual ~Tester(void) {}

  // functionality

  using Worker::process;

  /* Handles the TestMUS work item -- tests (group)MUS for correctness.
   */
  virtual bool process(TestMUS& tm);

  /* Handles the TestIrr work item -- tests irrendant subformulas for correctness.
   */
  virtual bool process(TestIrr& ti);

  /* Handles the TestVMUS work iterm -- test VMUS for correctness. 
   */
  virtual bool process(TestVMUS& tm);

protected:
  
  IDManager& _imgr;             // id manager

  ToolConfig& config;           // configuration (name is good for macros)

};

#endif /* _MUS_TESTER_HH */

/*----------------------------------------------------------------------------*/
