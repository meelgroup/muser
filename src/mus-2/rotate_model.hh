//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        rotate_model.hh
 *
 * Description: Class definition and implementation of work item for doing model 
 *              rotation.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _ROTATE_MODEL_H
#define _ROTATE_MODEL_H 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "types.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  RotateModel
 *
 * Purpose: A work item for group-based model rotation.
 *
 * Notes:
 *      1. The occ list inside groupset may be updated (cleaned up) on the fly.
 *
\*----------------------------------------------------------------------------*/

class RotateModel : public WorkItem {

public:     // Lifecycle

  RotateModel(MUSData& md)
    : _md(md), _pmodel(NULL), _owns_model(false), _collect_ft_gids(false) {}

  virtual ~RotateModel(void) {
    if (_owns_model && _pmodel != NULL) delete _pmodel;
  }

public:     // Parameters

  const MUSData& md(void) const { return _md; }
  MUSData& md(void) { return _md; }

  GID gid(void) const { return _gid; }
  void set_gid(GID gid) { _gid = gid; }

  const IntVector& model(void) const { return *_pmodel; }

  /* If make_copy is true will make its own copy of model.
   */
  void set_model(const IntVector& model, bool make_copy = false) {
    if (make_copy) {
      // make its own copy
      if (_pmodel == NULL) {
        _pmodel = new IntVector();
      } else
        _pmodel->clear();
      _pmodel->resize(model.size());
      copy(model.begin(), model.end(), _pmodel->begin());
      _owns_model = true;
    } else {
      // no copy, but if there was a copy before, get rid of it
      if (_owns_model && (_pmodel != NULL))
        delete _pmodel;
      _pmodel = const_cast<IntVector*>(&model); // model is never modified when
                                                // its not owned
      _owns_model = false;
    }
  }

  bool collect_ft_gids(void) const { return _collect_ft_gids; }
  void set_collect_ft_gids(bool cf) { _collect_ft_gids = cf; }

public:     // Results

  const GIDSet& nec_gids(void) const { return _nec_gids; }
  GIDSet& nec_gids(void) { return _nec_gids; }

  /* Returns the version of MUSData the results are for */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

  const GIDSet& ft_gids(void) const { return _ft_gids; }
  GIDSet& ft_gids(void) { return _ft_gids; }

public:     // Reset/recycle

  virtual void reset(void) {
    WorkItem::reset(); 
    _nec_gids.clear(); 
    _version = 0;
    _ft_gids.clear();
    if (_owns_model)
      _pmodel->clear();
  }

protected:

  // parameters

  MUSData& _md;                              // MUS data

  GID _gid;                                  // the group to rotate

  IntVector* _pmodel;                        // pointer to model

  bool _owns_model;                          // if true the model vector is owned

  bool _collect_ft_gids;                     // if true, collect "fasttrack" gids
  
  // results

  GIDSet _nec_gids;                          // GIDs of additional necessary groups

  unsigned _version;                         // the version of MUSData this result is for

  GIDSet _ft_gids;                           // GIDs of "fasttrack" groups

};

#endif /* _ROTATE_MODEL_H */

/*----------------------------------------------------------------------------*/
