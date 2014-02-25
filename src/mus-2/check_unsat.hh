/*----------------------------------------------------------------------------*\
 * File:        check_unsat.hh
 *
 * Description: Class declaration and implementation of work item for checking 
 *              unsatisfiability of a group set.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _CHECK_UNSAT_HH
#define _CHECK_UNSAT_HH 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  CheckUnsat
 *
 * Purpose: A work item for testing a group set for (un)satisfiability.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class CheckUnsat : public WorkItem {

public:     // Lifecycle

  CheckUnsat(const MUSData& md)     
    : _md(md), _unsat(false) {}

  virtual ~CheckUnsat(void) {};

public:     // Parameters

  const MUSData& md(void) const { return _md; }

public:   // Results

  bool is_unsat(void) const { return _unsat; }
  void set_unsat(void) { _unsat = true; }

protected:

  // parameters

  const MUSData& _md;                  // MUS data

  bool _unsat;                   // will be set to false if group set is SAT
  
};

#endif /* _TRIM_GROUP_SET_HH */

/*----------------------------------------------------------------------------*/
