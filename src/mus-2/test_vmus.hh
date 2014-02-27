/*----------------------------------------------------------------------------*\
 * File:        test_vmus.hh
 *
 * Description: Class declaration and implementation of work item for testing
 *              a VMUS of a groupset.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _TEST_VMUS_HH
#define _TEST_VMUS_HH 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  TestVMUS
 *
 * Purpose: A work item for testing a VMUS of a group set.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class TestVMUS : public WorkItem {

  friend class Tester;

public:     // Lifecycle

  enum Result { UNKNOWN = 0, UNSAT_VMU, UNSAT_NOTVMU, SAT };

  TestVMUS(MUSData& md)       // note that the reference is not constant
    : _md(md), _result(UNKNOWN), _cpu_time(0), _sat_calls(0), _rot_groups(0) {}

  virtual ~TestVMUS(void) {};

public: // Parameters

  MUSData& md(void) const { return _md; }

public: // Result
  
  Result result(void) const { return _result; }

  std::string result_string(void) const {
    std::ostringstream outs;
    switch (result()) {
    case UNSAT_VMU: 
      outs << "UNSAT_VMU (variable minimally unsatisfiable)"; break;
    case UNSAT_NOTVMU:
      outs << "UNSAT_NOTVMU (unsatisfiable, but " 
           << unnec_groups() << " variable groups are unnecessary)"; break;
    case SAT:
      outs << "SAT (satisfiable)"; break;
    case UNKNOWN:
      outs << "UNKNOWN"; break;
    }
    return outs.str();
  }

public: // Statistics

  /* Returns the elapsed CPU time (seconds) */
  double cpu_time(void) const { return _cpu_time; }

  /* Returns the number of calls to SAT solver during testing */
  unsigned sat_calls(void) const { return _sat_calls; }

  /* Returns the number of groups detected using model rotation during testing */
  unsigned rot_groups(void) const { return _rot_groups; }

  /* Returns the number of unnecessary groups in case of UNSAT_NONMU */
  unsigned unnec_groups(void) const { return _unnec_groups; }

protected:

  MUSData& _md;                  // MUS data

  Result _result;                // true when it is an MUS

  double _cpu_time;             // elapsed CPU time (seconds) for testing

  unsigned _sat_calls;          // number of calls to SAT solver

  unsigned _rot_groups;         // groups detected by model rotation (if enabled)

  unsigned _unnec_groups;       // number of unnecessary groups (if UNSAT_NONMU)

};

#endif /* _COMPUTE_MUS_HH */

/*----------------------------------------------------------------------------*/
