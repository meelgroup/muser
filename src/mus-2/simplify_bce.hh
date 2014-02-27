/*----------------------------------------------------------------------------*\
 * File:        simplify_bce.hh
 *
 * Description: Class definition and implementation of a work item for BCE- 
 *              based simplification.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _SIMPLIFY_BCE_HH
#define _SIMPLIFY_BCE_HH 1

#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  SimplifyBCE
 *
 * Purpose: A work item for BCE-based simplification of group set. The work 
 *          item has a parameter that tell whether the simplification is done
 *          destructively (i.e. group set itself is modified, e.g. during pre-
 *          processing), or not (i.e. the fields in MUSData are modified, but
 *          the group-set is not, e.g. during in-processing).
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class SimplifyBCE : public WorkItem {

public:     // Lifecycle

  SimplifyBCE(MUSData& md)          
    : _md(md), _cpu_time(0), _rcl_count(0), _rg_count(0) {}

  virtual ~SimplifyBCE(void) {}

public:     // Parameters

  MUSData& md(void) const { return _md; }

  /* true if the simplification is destructive */
  bool destructive(void) const { return _destr; }
  void set_destructive(bool destr) { _destr = destr; }

  /* if true, blocked clauses are moved to g0, instead of removed */
  bool blocked_2g0(void) const { return _2g0; }
  void set_blocked_2g0(bool b2g0 = true) { _2g0 = b2g0; }

  /* if true, group 0 clauses are ignored during BCE (not sound, in general) */
  bool ignore_g0(void) const { return _ig0; }
  void set_ignore_g0(bool ig0 = true) { _ig0 = ig0; }

public:     // Results

  /* Returns the version of MUSData the results are for */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

public:     // Statistics 

  /* The elapsed CPU time (seconds) */
  double& cpu_time(void) { return _cpu_time; }
  double cpu_time(void) const { return _cpu_time; }

  /* The number of removed (or moved to g0) clauses */
  unsigned& rcl_count(void) { return _rcl_count; }
  unsigned rcl_count(void) const { return _rcl_count; }

  /* The number of removed (or moved to g0) groups */
  unsigned& rg_count(void) { return _rg_count; }
  unsigned rg_count(void) const { return _rg_count; }

public:     // Reset/recycle

  virtual void reset(void) {
    _cpu_time = 0; _rcl_count = 0; _rg_count = 0;
  }

protected:

  // parameters

  MUSData& _md;             // MUS data

  bool _destr = true;       // if true, simplification should be done destructively

  bool _2g0 = false;        // if true, move blocked clauses into g0 instead of
                            // removing them

  bool _ig0 = false;        // if true, ignore g0 clauses (unsound, in general)

  // results

  unsigned _version = 0;    // the version of MUSData this result is for

  // stats

  double _cpu_time = 0;     // elapsed CPU time (seconds) for extraction

  unsigned _rcl_count = 0;  // number of removed clauses

  unsigned _rg_count = 0;   // number of removed groups

};

#endif /* _SIMPLIFY_BCE_HH */

/*----------------------------------------------------------------------------*/
