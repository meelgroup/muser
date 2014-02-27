/*----------------------------------------------------------------------------*\
 * File:        test_irr.hh
 *
 * Description: Class declaration and implementation of work item for testing
 *              an irredundant subset of a groupset.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _TEST_IRR_HH
#define _TEST_IRR_HH 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  TestIrr
 *
 * Purpose: A work item for testing an irredundant subset of a group set.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class TestIrr : public WorkItem {

  friend class Tester;

public:     // Lifecycle

  enum Result { 
    UNKNOWN = 0,
    IRRED_CORRECT,      // irredundant, and all removed clauses are implied
    IRRED_INCORRECT,    // irredundant, some removed clauses are not implied
    RED };              // redundant

  TestIrr(MUSData& md)       // note that the reference is not constant
    : _md(md), _result(UNKNOWN), _cpu_time(0), _sat_calls(0), _red_groups(0), 
      _nonimpl_groups(0) {}

  virtual ~TestIrr(void) {};

public: // Parameters

  MUSData& md(void) const { return _md; }

public: // Result
  
  Result result(void) const { return _result; }

  std::string result_string(void) const {
    std::ostringstream outs;
    switch (result()) {
    case IRRED_CORRECT:
      outs << "IRRED_CORRECT (irredundant and all removed clauses are implied)"; break;
    case IRRED_INCORRECT:
      outs << "IRRED_INCORRECT (irredundant, but "
           << nonimpl_groups() << " removed clauses are not implied)"; break;
    case RED:
      outs << "RED (redundant, " 
           << red_groups() << " clauses are redundant)"; break;
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

  /* Returns the number of redundant groups in case of RED */
  unsigned red_groups(void) const { return _red_groups; }

  /* Returns the number of non-implied groups in case of IRRED_INCORRECT */
  unsigned nonimpl_groups(void) const { return _nonimpl_groups; }

protected:

  MUSData& _md;                  // MUS data

  Result _result;                // true when it is an MUS

  double _cpu_time;             // elapsed CPU time (seconds) for testing

  unsigned _sat_calls;          // number of calls to SAT solver

  unsigned _red_groups;         // number of redundant groups (if RED)

  unsigned _nonimpl_groups;     // number of nonimpied groups (if IRRED_INCORRECT)
};

#endif /* _TEST_IRR_HH */

/*----------------------------------------------------------------------------*/
