/*----------------------------------------------------------------------------*\
 * File:        trim_group_set.hh
 *
 * Description: Class declaration and implementation of work item for trimming
 *              a group set.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _TRIM_GROUP_SET_HH
#define _TRIM_GROUP_SET_HH 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  TrimGroupSet
 *
 * Purpose: A work item for trimming a group set.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class TrimGroupSet : public WorkItem {

public:     // Lifecycle

  // note that the reference to the groupset is not constant - the groupset 
  // will be modified when this item is processed
  TrimGroupSet(MUSData& md)     
    : _md(md), _trim_fixpoint(false), _iter_limit(0), _pct_limit(0), 
      _unsat(false) {}

  virtual ~TrimGroupSet(void) {};

public:     // Parameters

  MUSData& md(void) const { return _md; } // also the result

  bool trim_fixpoint(void) const { return _trim_fixpoint; }
  void set_trim_fixpoint(bool tfp) { _trim_fixpoint = tfp; }

  unsigned iter_limit(void) const { return _iter_limit; }
  void set_iter_limit(unsigned iter_limit) { _iter_limit = iter_limit; }

  unsigned pct_limit(void) const { return _pct_limit; }
  void set_pct_limit(unsigned pct_limit) { _pct_limit = pct_limit; }

public:   // Results

  bool is_unsat(void) const { return _unsat; }
  void set_unsat(void) { _unsat = true; }

protected:

  // parameters

  MUSData& _md;                  // MUS data

  bool _trim_fixpoint;           // if true, then trim till fixpoint; otherwise, trim
                                 // by number of iterations (if not 0), otherwise trim
                                 // by percent (if not 0)

  unsigned _iter_limit;          // when _trim_by_iter, number of iterations to 
                                 // trim

  unsigned _pct_limit;           // when !_trim_by_iter, percentage of change 
                                 // to stop at (0 - till fix point)

  bool _unsat;                   // will be set to false if group set is SAT
  
};

#endif /* _TRIM_GROUP_SET_HH */

/*----------------------------------------------------------------------------*/
