/*----------------------------------------------------------------------------*\
 * File:        check_subset_status.hh
 *
 * Description: Class declaration and implementation of work item for checking 
 *              status of a subset of groups (aka "bunch") with respect to 
 *              group-set [ECAI-12 submission]
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _CHECK_SUBSET_STATUS_H
#define _CHECK_SUBSET_STATUS_H 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "types.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  CheckSubsetStatus
 *
 * Purpose: A work item for checking the status (necessary or not) of a subset 
 * of group  with respect to a group set.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class CheckSubsetStatus : public WorkItem {

public:     // Lifecycle

  /* The last parameter says whether this is a first check for this chunk;
   * note that gid is expected to be included in the chunk
   */
  CheckSubsetStatus(const MUSData& md, const GIDSet& subset)
    : _md(md), _subset(subset), _refine(false), _need_model(false), 
      _status(false) {}

  virtual ~CheckSubsetStatus(void) {};

public:     // Parameters

  const MUSData& md(void) const { return _md; }

  /* The subset of gids to check */
  const GIDSet& subset(void) const { return _subset; }

  /* If true, then in case none of the groups in the subset are necessary, find 
   * more unnecessary groups by refinement */
  bool refine(void) const { return _refine; }
  void set_refine(bool refine) { _refine = refine; }

  /* If true, then in case some of the groups in the subset are necessary, get
   * the witness (the model of the remainder */
  bool need_model(void) const { return _need_model; }
  void set_need_model(bool need_model) { _need_model = need_model; }

  /* If true, use redundancy removal (i.e. add negation of gids to SAT call) */
  bool use_rr(void) const { return _use_rr; }
  void set_use_rr(bool use_rr) { _use_rr = use_rr; }

public:     // Results

  /* True if some of the groups are necessary, false if not */
  bool status(void) const { return _status; }
  void set_status(bool status) { _status = status; }

  /* If not necessary, will contain the gids of the subset, plus some more 
   * gids from the remainder if refine() is true. */
  const GIDSet& unnec_gids(void) const { return _unnec_gids; }
  GIDSet& unnec_gids(void) { return _unnec_gids; }

  /* If nesessary and need_model() is true, this will refer to the model */
  const IntVector& model(void) const { return _model; }
  IntVector& model(void) { return _model; }

  /* Returns the version of MUSData the results are for - note that the 
   * version is incremented whenever groups are removed from the group set */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

public:     // Reset/recycle

  virtual void reset(void) {
    WorkItem::reset(); _status = false; _unnec_gids.clear();
    _model.clear(); _version = 0; 
  }

protected:

  // parameters

  const MUSData& _md;                        // MUS data

  const GIDSet& _subset;                     // the subset to test

  bool _refine;                              // if true add refined GIDs

  bool _need_model;                          // if true save model if SAT

  bool _use_rr;                              // if true use redundancy removal trick

  // results

  bool _status;                              // true if SAT, false if not

  GIDSet _unnec_gids;                        // GIDs of unnecessary groups

  IntVector _model;                          // model (if SAT and asked for it)

  unsigned _version;                         // the version of MUSData this result is for
};

#endif /* _CHECK_SUBSET_STATUS_H */

/*----------------------------------------------------------------------------*/
