/*----------------------------------------------------------------------------*\
 * File:        check_group_status.hh
 *
 * Description: Class declaration and implementation of work item for checking 
 *              status of a group with respect to group-set.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _CHECK_GROUP_STATUS_H
#define _CHECK_GROUP_STATUS_H 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "types.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  CheckGroupStatus
 *
 * Purpose: A work item for checking the status (necessary or not) of a group 
 * with respect to a group set. 
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class CheckGroupStatus : public WorkItem {

public:     // Lifecycle

  CheckGroupStatus(const MUSData& md, GID gid)
    : _md(md), _gid(gid) {}

  virtual ~CheckGroupStatus(void) {};

public:     // Parameters

  const MUSData& md(void) const { return _md; }

  /* The gid of the group to check */
  GID gid(void) const { return _gid; }
  void set_gid(GID gid) { _gid = gid; }

  /* If true, then in case the group is not necessary, find more by refinement */
  bool refine(void) const { return _refine; }
  void set_refine(bool refine) { _refine = refine; }

  /* If true, then in case the group is necessary, get the model of the remainder */
  bool need_model(void) const { return _need_model; }
  void set_need_model(bool need_model) { _need_model = need_model; }

  /* If true, use redundancy removal (i.e. add negation of gid to SAT call) */
  bool use_rr(void) const { return _use_rr; }
  void set_use_rr(bool use_rr) { _use_rr = use_rr; }

  /* If true, save the core as well */
  bool save_core(void) const { return _save_core; }
  void set_save_core(bool save_core) { _save_core = save_core; }

  /* Conflict limit for this call; -1 = no limit */
  LINT conf_limit(void) const { return _conf_limit; }
  void set_conf_limit(LINT cl) { _conf_limit = cl; }

  /* CPU limit for this call (secs); 0 = no limit */
  float cpu_limit(void) const { return _cpu_limit; }
  void set_cpu_limit(float cl) { _cpu_limit = cl; }

public:     // Results

  /* True if necessary, false if not */
  bool status(void) const { return _status; }
  void set_status(bool status) { _status = status; }

  /* If not necessary, will contain the gid of the group, plus some more if 
   * refine() is true, and use_rr() did not get in the way */
  const GIDSet& unnec_gids(void) const { return _unnec_gids; }
  GIDSet& unnec_gids(void) { return _unnec_gids; }

  /* If nesessary and need_model() is true, this will refer to the model */
  const IntVector& model(void) const { return _model; }
  IntVector& model(void) { return _model; }

  /* Returns the version of MUSData the results are for - note that the 
   * version is incremented whenever groups are removed from the group set */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

  /* If true, rr got in a way of refinement */
  const bool& tainted_core(void) const { return _tcore; }
  bool& tainted_core(void) { return _tcore; }

  /* Returns a read-only pointer to the core (all the way into SAT solver),
   * or nullptr if the outcome was SAT.
   */
  const GIDSet* pcore(void) { return &_core; }
  void set_pcore(const GIDSet* pcore) {
    _core.clear(); if (pcore != nullptr) _core = *pcore; } // TEMP slow !!!!

public:     // Reset/recycle

  virtual void reset(void) {
    WorkItem::reset(); _status = false; _unnec_gids.clear();
    _model.clear(); _version = 0; _tcore = false;
  }

protected:

  // parameters

  const MUSData& _md;                        // MUS data

  GID _gid;                                  // the group to test

  bool _refine = false;                      // if true add refined GIDs

  bool _need_model = false;                  // if true save model if SAT

  bool _use_rr = false;                      // if true use redundancy removal trick

  bool _save_core = false;                   // if true, save the core as well

  LINT _conf_limit = -1;                     // conflicts limit for this call

  float _cpu_limit = 0.0f;                   // cpu limit for this call

  // results

  bool _status = false;                      // true if SAT, false if not

  GIDSet _unnec_gids;                        // GIDs of unnecessary groups

  IntVector _model;                          // model (if SAT and asked for it)

  unsigned _version;                         // the version of MUSData this result is for

  bool _tcore;                               // when true, rr got in a way of refinement

  GIDSet _core;                              // TEMP: make direct access ! pointer to the most recent core
                                             // or nullptr if outcome was SAT
};

#endif /* _CHECK_GROUP_STATUS_H */

/*----------------------------------------------------------------------------*/
