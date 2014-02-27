/*----------------------------------------------------------------------------*\
 * File:        check_range_status.hh
 *
 * Description: Class declaration and implementation of work item for checking 
 *              status of a range of groups -- this is used mostly in insertion
 *              like MUS algorithms.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _CHECK_RANGE_STATUS_H
#define _CHECK_RANGE_STATUS_H 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "types.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  CheckRangeStatus
 *
 * Purpose: A work item for checking the status of a range of groups; used
 *          mostly in the insertion-based algorithms. The semantics are as
 *          follows: status() == true (SAT) if md.gset().n_gids() union
 *          the gids in the range [r_begin,r_end) are satisfiable. When used
 *          in MES mode, the r_allend points of the end of the whole 
 *          "working set", so that the negation can be formed properly.
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class CheckRangeStatus : public WorkItem {

public:     // Lifecycle

  /* The last parameter says whether this is a first check for this chunk;
   * note that gid is expected to be included in the chunk
   */
  CheckRangeStatus(const MUSData& md)
    : _md(md) {}

  virtual ~CheckRangeStatus(void) {};

public:     // Parameters

  const MUSData& md(void) const { return _md; }

  /* The begining of the range */
  void set_begin(const GIDVector::const_iterator& pbegin) { _pbegin = pbegin; }
  const GIDVector::const_iterator& begin(void) { return _pbegin; }

  /* The end of the range */
  void set_end(const GIDVector::const_iterator& pend) { _pend = pend; }
  const GIDVector::const_iterator& end(void) { return _pend; }

  /* The end of all */
  void set_allend(const GIDVector::const_iterator& pallend) { _pallend = pallend; }
  const GIDVector::const_iterator& allend(void) { return _pallend; }

  /* If true, then in case none of the groups in the subset are necessary, find 
   * more unnecessary groups by refinement */
  bool refine(void) const { return _refine; }
  void set_refine(bool refine) { _refine = refine; }

  /* If true, then in case some of the groups in the subset are necessary, get
   * the witness (the model of the remainder); if p_model != nullptr the model 
   * will be saved into the vector pointed by p_model
   */
  bool need_model(void) const { return _need_model; }
  void set_need_model(bool need_model, IntVector* p_model = nullptr) { 
    _need_model = need_model; _p_model = p_model; }
  
  /* If true, the negation of [end(),allend()) is added prior to the SAT
   * check -- this is used in redundancy removal mode. */
  bool add_negation(void) const { return _add_negation; }
  void set_add_negation(bool add_negation) { _add_negation = add_negation; }

public:     // Results

  /* True if some of the groups are necessary, false if not */
  bool status(void) const { return _status; }
  void set_status(bool status) { _status = status; }

  /* If not necessary, will contain the gids of the subset, plus some more 
   * gids from the remainder if refine() is true. */
  const GIDSet& unnec_gids(void) const { return _unnec_gids; }
  GIDSet& unnec_gids(void) { return _unnec_gids; }

  /* If nesessary and need_model() is true, this will refer to the model */
  const IntVector& model(void) const { return this->model(); }
  IntVector& model(void) { return (_p_model == nullptr) ? _model : *_p_model; }

  /* Returns the version of MUSData the results are for - note that the 
   * version is incremented whenever groups are removed from the group set */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

public:     // Reset/recycle

  virtual void reset(void) {
    WorkItem::reset(); _status = false; _unnec_gids.clear();
    model().clear(); _version = 0; 
  }

protected:

  // parameters

  const MUSData& _md;                        // MUS data

  GIDVector::const_iterator _pbegin;         // iterator to the begining of the range

  GIDVector::const_iterator _pend;           // iterator past the end of the range

  GIDVector::const_iterator _pallend;        // iterator past the end of all GIDs
   
  bool _refine = false;                      // if true add refined GIDs

  bool _need_model = false;                  // if true save model if SAT

  bool _add_negation = false;                // if true, add negation of [_pend, p_allend]
                                             // prior to SAT check (for redundancy removal)

  // results

  bool _status = false;                      // true if SAT, false if not

  GIDSet _unnec_gids;                        // GIDs of unnecessary groups among
                                             // those in the range

  IntVector _model;                          // model (if SAT and asked for it)

  IntVector* _p_model = nullptr;             // or this one for the model

  unsigned _version = 0;                     // the version of MUSData this result is for

};

#endif /* _CHECK_RANGE_STATUS_H */

/*----------------------------------------------------------------------------*/
