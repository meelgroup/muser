/*----------------------------------------------------------------------------*\
 * File:        check_group_status_chunk.hh
 *
 * Description: Class declaration and implementation of work item for checking 
 *              status of a group with respect to group-set in chunk-based
 *              approach.
 *
 * Author:      antonb
 * 
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _CHECK_GROUP_STATUS_CHUNK_H
#define _CHECK_GROUP_STATUS_CHUNK_H 1

#include "basic_group_set.hh"
#include "mus_data.hh"
#include "types.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  CheckGroupStatusChunk
 *
 * Purpose: A work item for checking the status (necessary or not) of a group 
 * with respect to a group set and a chunk. 
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class CheckGroupStatusChunk : public WorkItem {

public:     // Lifecycle

  /* The last parameter says whether this is a first check for this chunk;
   * note that gid is expected to be included in the chunk
   */
  CheckGroupStatusChunk(const MUSData& md, GID gid, const GIDSet& chunk, 
                        bool first = true)
    : _md(md), _gid(gid), _chunk(chunk), _first(first), _refine(false), 
      _need_model(false), _status(false) {}

  virtual ~CheckGroupStatusChunk(void) {};

public:     // Parameters

  const MUSData& md(void) const { return _md; }

  /* The gid of the group to check */
  GID gid(void) const { return _gid; }
  void set_gid(GID gid) { _gid = gid; }

  /* The chunk: note gid \in chunk is an invariant */
  const GIDSet& chunk(void) const { return _chunk; }

  /* If true this is the first gid in the chunk */
  bool first(void) const { return _first; }
  void set_first(bool first) { _first = first; }

  /* If true, then in case the group is not necessary, find more unnecessary
   * groups *in the chunk* by refinement */
  bool refine(void) const { return _refine; }
  void set_refine(bool refine) { _refine = refine; }

  /* If true, then in case the group is necessary, get the model of the remainder */
  bool need_model(void) const { return _need_model; }
  void set_need_model(bool need_model) { _need_model = need_model; }

public:     // Results

  /* True if necessary, false if not */
  bool status(void) const { return _status; }
  void set_status(bool status) { _status = status; }

  /* If not necessary, will contain the gid of the group, plus some more gids
   * from the chunk if refine() is true. */
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
    WorkItem::reset(); _status = false; _unnec_gids.clear(); _first = false;
    _model.clear(); _version = 0; 
  }

protected:

  // parameters

  const MUSData& _md;                        // MUS data

  GID _gid;                                  // the group to test

  const GIDSet& _chunk;                      // the chunk to test wrt

  bool _first;                               // if true, _gid is the first from chunk

  bool _refine;                              // if true add refined GIDs

  bool _need_model;                          // if true save model if SAT

  // results

  bool _status;                              // true if SAT, false if not

  GIDSet _unnec_gids;                        // GIDs of unnecessary groups

  IntVector _model;                          // model (if SAT and asked for it)

  unsigned _version;                         // the version of MUSData this result is for
};

#endif /* _CHECK_GROUP_STATUS_CHUNK_H */

/*----------------------------------------------------------------------------*/
