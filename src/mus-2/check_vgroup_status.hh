/*----------------------------------------------------------------------------*\
 * File:        check_vgroup_status.hh
 *
 * Description: Class declaration and implementation of work item for checking 
 *              status of a group of variable with respect to group-set.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _CHECK_VGROUP_STATUS_H
#define _CHECK_VGROUP_STATUS_H 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "types.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  CheckVGroupStatus
 *
 * Purpose: A work item for checking the status (necessary or not) of a group 
 * of variables with respect to a group set. 
 *
 * Notes:
 *      1. Not ready for MT
 *
\*----------------------------------------------------------------------------*/

class CheckVGroupStatus : public WorkItem {

public:     // Lifecycle

  CheckVGroupStatus(const MUSData& md, GID vgid)
    : _md(md), _vgid(vgid), _refine(false), _need_model(false), 
      _use_rr(false), _status(false), _version(0) {}

  virtual ~CheckVGroupStatus(void) {};

public:     // Parameters

  const MUSData& md(void) const { return _md; }

  /* The gid of the variable group to check */
  GID vgid(void) const { return _vgid; }
  void set_vgid(GID vgid) { _vgid = vgid; }

  /* If true, then in case the group is not necessary, find more by refinement */
  bool refine(void) const { return _refine; }
  void set_refine(bool refine) { _refine = refine; }

  /* If true, then in case the group is necessary, get the model of the remainder */
  bool need_model(void) const { return _need_model; }
  void set_need_model(bool need_model) { _need_model = need_model; }

  /* If true, use redundancy removal (i.e. add negation of the group to SAT call) */
  bool use_rr(void) const { return _use_rr; }
  void set_use_rr(bool use_rr) { _use_rr = use_rr; }

public:     // Results

  /* True if necessary, false if not */
  bool status(void) const { return _status; }
  void set_status(bool status) { _status = status; }

  /* If not necessary, will contain the gid of the variable group, plus some 
   * more if refine() is true, and use_rr() did not get in the way */
  const GIDSet& unnec_vgids(void) const { return _unnec_vgids; }
  GIDSet& unnec_vgids(void) { return _unnec_vgids; }

  /* If nesessary and need_model() is true, this will refer to the model */
  const IntVector& model(void) const { return _model; }
  IntVector& model(void) { return _model; }

  /* Returns the version of MUSData the results are for - note that the 
   * version is incremented whenever groups are removed from the group set */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

  const GIDSet& ft_vgids(void) const { return _ft_vgids; }
  GIDSet& ft_vgids(void) { return _ft_vgids; }

public:     // Reset/recycle

  virtual void reset(void) {
    WorkItem::reset(); _status = false; _unnec_vgids.clear(); 
    _model.clear(); _version = 0; _ft_vgids.clear();
  }

protected:

  // parameters

  const MUSData& _md;                        // MUS data

  GID _vgid;                                 // the group to test

  bool _refine;                              // if true add refined GIDs

  bool _need_model;                          // if true save model if SAT

  bool _use_rr;                              // if true use redundancy removal trick

  // results

  bool _status;                              // true if SAT, false if not

  GIDSet _unnec_vgids;                       // GIDs of unnecessary groups

  IntVector _model;                          // model (if SAT and asked for it)

  unsigned _version;                         // the version of MUSData this result is for

  GIDSet _ft_vgids;                          // GIDs of "fasttrack" groups

};

#endif /* _CHECK_VGROUP_STATUS_H */

/*----------------------------------------------------------------------------*/
