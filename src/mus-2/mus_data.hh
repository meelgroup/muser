/*----------------------------------------------------------------------------*\
 * File:        mus_data.hh
 *
 * Description: Class definition and implementation of MUSData container
 *
 * Author:      antonb
 * 
 * Notes:
 *
 * 1. MUSData is 1-to-1 with the initial group set
 * 2. Removed lists of clauses - everything is in terms of groups now.
 *    NOTE: if space becomes an issue, combine list and set into one set ordered
 *    by timestamp
 *
 *                                          Copyright (c) 2011-2012, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _MUS_DATA_HH
#define _MUS_DATA_HH 1

#include <algorithm>
#include <iostream>
#include <list>
#include "globals.hh"
#include "basic_group_set.hh"

/*----------------------------------------------------------------------------*\
 * Class: MUSData
 *
 * Purpose: Container for MUS extraction-related data
 *
 * Description:
 *
 * MUSData is a container for various information related to MUS extraction:
 * the group set, the removed groups, final groups, MUS groups etc. 
 *
\*----------------------------------------------------------------------------*/

class MUSData {

public:

  MUSData(BasicGroupSet& gset) : _gset(gset) {}

  virtual ~MUSData(void) {}

public:    // Accessors

  /* Returns a refence to the underlying group set */
  BasicGroupSet& gset(void) { return _gset; }
  const BasicGroupSet& gset(void) const { return _gset; }

  /* Returns a reference to the set of group IDs of removed groups. Thus,
   * effectively, the instance represented by MUSData consists of
   * gset() \setminus r_gids()
   */
  GIDSet& r_gids(void) { return _r_gids; }
  const GIDSet& r_gids(void) const { return _r_gids; }

  /* Returns a reference to the set of groups that are known to be in all high-
   * level MUSes gset() \setminus r_gids()
   */
  GIDSet& nec_gids(void) { return _nec_gids; }
  const GIDSet& nec_gids(void) const { return _nec_gids; }

  /* Returns a reference to the list of removed groups, with the most recent in
   * the front
   */
  GIDList& r_list(void) { return _r_list; }
  const GIDList& r_list(void) const { return _r_list; }

  /* Returns a reference to the list of finalized groups, with the most recent
   * in front
   */
  GIDList& f_list(void) { return _f_list; }
  const GIDList& f_list(void) const { return _f_list; }

  /* Returns the actual size of the instance (number of non-removed groups)
   */
  unsigned real_gsize(void) const { return _gset.gsize() - _r_gids.size(); }

public:    // Updates

  /* Marks gid as removed: puts it into r_gids() and r_list(), and updates the
   * gset(). Also if has resolution graph, clears the vertices from the graph.
   */
  void mark_removed(GID gid) {
    assert(!r(gid) && !nec(gid));
    _r_gids.insert(gid);
    _r_list.push_front(gid);
    _gset.remove_group(gid);
  }

  /* Marks gid as necessary: puts it into nec_gids() and f_list()               
   */
  void mark_necessary(GID gid) {
    assert(!r(gid) && !nec(gid));
    _nec_gids.insert(gid);
    _f_list.push_front(gid);
  }

  /* Clears the lists */
  void clear_lists(void) { _f_list.clear(); _r_list.clear(); }

public:    // Status checks

  /* True if group with gid is removed */
  bool r(const GID& gid) const { return _r_gids.find(gid) != _r_gids.end(); }

  /* True if group with gid is necessary */
  bool nec(const GID& gid) const { return _nec_gids.find(gid) != _nec_gids.end(); }

  /* True if group with gid is untested (neither necessary nor removed */
  bool untested(const GID& gid) const { return !r(gid) && !nec(gid); }
 
public:    // Output functions

  /* Writes out the instance in competition format (i.e. "v " followed by 
   * group-ids) 
   */
  std::ostream& write_comp(std::ostream& out) {
    out << "v ";
    std::for_each(_gset.gbegin(), _gset.gend(), [&](GID gid) {
        if (gid && !r(gid)) out << gid << " "; });
    out << "0" << std::endl;
    return out;
  }

  /* Writes out the instance in cnf format -- this presumes that every group
   * contains exactly one clause, and that there is no group 0. Will throw 
   * logic_error if this is not the case.
   */
  std::ostream& write_cnf(std::ostream& out) {
    // check if finished, if not, add a comment
    if (real_gsize() != _nec_gids.size())
      out << "c WARNING: MUSer2 did not finish extraction; "
          << "this is an over-approximation of the result." << endl;
    out << "p cnf " << _gset.max_var() << " " 
        << (_gset.gsize() - _r_gids.size()) << std::endl;
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid) {
      if (*pgid == 0) 
        throw std::logic_error("MUSData::write_cnf(): found group 0,"
                               " this is not CNF");
      if (!r(*pgid)) {
        BasicClauseVector& gcl = pgid.gclauses();
        if (gcl.size() != 1)
          throw std::logic_error("MUSData::write_cnf(): found non-singleton "   
                                 "group, this is not CNF");
        assert(!gcl[0]->removed());
        gcl[0]->awrite(out); 
        out << std::endl;
      }
    }
    return out;
  }

  /* Writes out the instance in gcnf format.
   */
  std::ostream& write_gcnf(std::ostream& out) {
    // check if finished, if not, add a comment
    if (real_gsize()-1 != _nec_gids.size())
      out << "c WARNING: MUSer2 did not finish extraction; "
          << "this is an over-approximation of the result." << endl;    
    unsigned r_clauses = 0; // number of removed clauses
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid)
      if (r(*pgid))
        r_clauses += pgid.gclauses().size();
    out << "p gcnf " << _gset.max_var()
        <<  " " << _gset.size() - r_clauses
        <<  " " << _gset.max_gid() << std::endl;
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid) {
      if (!r(*pgid)) {
        BasicClauseVector& gcl = pgid.gclauses();
        for (ClVectIterator pcl = gcl.begin(); pcl != gcl.end(); ++pcl)
          if (!(*pcl)->removed()) {
            out << "{" << *pgid << "} ";
            (*pcl)->awrite(out);
            out << std::endl;
          }
      }
    }
    return out;
  }

public:         // Miscellaneous

  /* Makes all groups except group 0 unnecessary -- this is useful when g0 is 
   * deemed unsatisfiable.
   */
  void make_empty_gmus(void) {
    _r_list.clear();
    for (gset_iterator pgid = _gset.gbegin(); pgid != _gset.gend(); ++pgid) {
      _r_gids.insert(*pgid);
      _r_list.push_back(*pgid);
    }
    _nec_gids.clear();
  }

protected:

  BasicGroupSet& _gset;        // the group set

  GIDSet _r_gids;              // removed GIDs

  GIDSet _nec_gids;            // necessary GIDs of _gset \setminus _r_gids

  GIDList _r_list;             // ordered list of removed GIDs: most recent first

  GIDList _f_list;             // ordered list of finalized GIDs: most recent first

};

#endif /* _MUS_DATA_H */

/*----------------------------------------------------------------------------*/
