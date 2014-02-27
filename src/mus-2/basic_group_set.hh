//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        basic_group_set.hh
 *
 * Description: Container for a set of clauses partitioned into disjoint groups.
 *              Supports
 *               - direct access to clauses of a particular group
 *               - iteration over groups
 *               - iteration over clauses
 *              Also related classes and types are defined here (e.g. GID and
 *              containters of GIDs)
 *
 * Author:      antonb
 * 
 * Notes:
 *
 * 1. This implementation is tailored and optimized for MUSer2. As a result many 
 * (most) of the methods of BasicClauseSet are not implemented, and those that 
 * are may assume certain usage scenario. When this is the case, the comments
 * will (hopefully) indicate this.
 *
 * 2. This implementation is designed primarily for GCNFs, as such normal CNFs 
 * are treated as sets of single-clause groups with no group 0. Its plausible 
 * that a version of this class optimized for CNF will need to be created (TODO:
 * make sure to profile first, it might not be a big issue).
 *
 * 3. BasicGroupSet is *not* thread-safe, and does not do any locking by itself
 *
 * 4. At the moment it is a "read-only" object -- I will allow "fake removals"
 * soon.
 *
 * 5. The implementation has been changed over from map-based to vector-based,
 * so some of the naming conventions are kept consisent with maps.
 *
 * 6. The clause pointers are dublicated -- one copy is in the vector of clauses
 * accessed via BasicGroupSet::begin() and BasicGroupSet::end() methods, while
 * the other copy is in the vector of clauses access through 
 * BasicClauseSet::gclauses() and gset_iterator::gclauses() method. This, of
 * course, is a waste, however this is done to make the class backward compatible
 * with MUSer2 code I developed so far. Hopefully, at some point this will be
 * re-written (or not, but implemented property in "competition" versions).
 *
 * 7. The group-set has its own copy of ClauseRegistry -- i.e. not the static
 * one.
 *
 * Revision:    $Id$.
 *
 *                       Copyright (c) 2009-2011, Anton Belov,Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _BASIC_GROUP_SET_H
#define _BASIC_GROUP_SET_H 1

#include <ext/hash_map>       // Location of STL hash extensions
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "globals.hh"
#include "basic_clause.hh"
#include "cl_functors.hh"
#include "cl_types.hh"
#include "cl_registry.hh"
#include "mus_config.hh"
#include "occs_list.hh"


/** Group ID to clause vector map */
typedef std::map<GID, BasicClauseVector> GID2ClVMap;

/** Group ID to integer map (e.g. for indicators) */
typedef __gnu_cxx::hash_map<GID, LINT, IntHash, IntEqual> GID2IntMap;

/** Vector of GIDs and related iterators 
 */
typedef std::vector<GID> GIDVector;
typedef GIDVector::iterator GIDVectorIterator;
typedef GIDVector::const_iterator GIDVectorCIterator;

/** Set of GIDs and related iterators 
 */
typedef std::set<GID> GIDSet;
typedef GIDSet::iterator GIDSetIterator;
typedef GIDSet::const_iterator GIDSetCIterator;
// output
inline std::ostream& operator<<(std::ostream& out, const GIDSet& gs) {
  out << "{ "; 
  copy(gs.begin(), gs.end(), ostream_iterator<ULINT>(out, " "));
  out << "}";
  return out;
}
// hasher
class GIDSetHash {
public:
  ULINT operator()(const GIDSet& gs) const {  
    // simply XOR the values
    switch (gs.size()) {
    case 0: return (ULINT)-1L;
    case 1: return (ULINT)*gs.begin();
    }
    ULINT res = 0;
    for (GIDSet::const_iterator pg = gs.begin(); pg != gs.end(); ++pg)
      res = res ^ *pg;
    return res;
  }
};

/** Hash set of GIDs and related iterators 
 */
typedef __gnu_cxx::hash_set<GID, IntHash, IntEqual> GIDHSet;
typedef GIDHSet::iterator GIDHSetIterator;
typedef GIDHSet::const_iterator GIDHSetCIterator;
// output
inline std::ostream& operator<<(std::ostream& out, const GIDHSet& gs) {
  out << "{ "; 
  copy(gs.begin(), gs.end(), ostream_iterator<ULINT>(out, " "));
  out << "}";
  return out;
}
// hasher
class GIDHSetHash {
public:
  ULINT operator()(const GIDHSet& gs) const {  
    // simply XOR the values
    switch (gs.size()) {
    case 0: return (ULINT)-1L;
    case 1: return (ULINT)*gs.begin();
    }
    ULINT res = 0;
    for (GIDHSet::const_iterator pg = gs.begin(); pg != gs.end(); ++pg)
      res = res ^ *pg;
    return res;
  }
};

/** List of GIDs and related iterators 
 */
typedef std::list<GID> GIDList;
typedef GIDList::iterator GIDListIterator;
typedef GIDList::const_iterator GIDListCIterator;

/** Vector of variables */
typedef std::vector<ULINT> VarVector;

/*----------------------------------------------------------------------------*\
 * Class: BasicGroupSet
 *
 * Purpose: Container for a set of clauses partitioned into disjoint groups
 *
 * Notes: 
 *      1. See notes at the begining of this file.
 *      2. With the introduction of VMUSes group set has an extra layer of
 *      mapping from GIDs of variable groups to variables. When in VMUS mode
 *      the occlist is contructed and used to get to the actual clauses; if
 *      this becomes a problem, the access can be streamlined.
 *      3. TODO: Iterator over GIDMap and VGIDMap is made into template, maybe
 *      I should just make the map itself a template ? 
 *
\*----------------------------------------------------------------------------*/

class BasicGroupSet {

protected:      // Some internal types

  // info for groups of clauses
  struct GroupInfo {
    BasicClauseVector content;   // all clauses for this group
    ULINT a_count;               // the number of active clauses in the group
    GroupInfo(void) : a_count(0) {}
  };
  typedef std::vector<GroupInfo*> GIDMap;   // where everything is; index = gid

  // info for groups of variables
  struct VGroupInfo {
    VarVector content;           // all variables for this group
    ULINT a_count;               // the number of active variables in the group
    VGroupInfo(void) : a_count(0) {}
  };
  typedef std::vector<VGroupInfo*> VGIDMap; // where everything is; index = gid

  /* Iterator over a group map, i.e. vector of pointers indexed by GID that 
   * contains NULL pointer for empty groups, and otherwise a structure that
   * has two fields: content and a_count (e.g. GIDMap or VGIDMap). The iterator
   * skips empty groups; this is packaged as a template and used for example for
   * groups of clauses and groups of variables. Template parameters:
   *   GM -- the map class (GIDMap or VGIDMap)
   *   Cont -- the type that represents group's content (e.g. vector of clauses)
   * TODO: make a const iterator as well
   */
  template <class GM, class Cont>
  class gmap_iter_tmpl
    : public std::iterator<std::bidirectional_iterator_tag, GID> {
  public:
    typedef typename GM::iterator GM_iter;
    // lifecycle
    gmap_iter_tmpl(GM& gmap, const GM_iter& iter) : 
      _iter(iter), _begin(gmap.begin()), _end(gmap.end()) {
      // advance to the first non-empty group
      while ((_iter != _end) && *_iter == NULL) ++_iter;
    }
    // relational
    bool operator==(const gmap_iter_tmpl& o) const { return _iter == o._iter; }
    bool operator!=(const gmap_iter_tmpl& o) const { return _iter != o._iter; }
    // movement forward (no post-increment)
    gmap_iter_tmpl& operator++(void) {
      if (_iter == _end) throw std::out_of_range("gmap_iter_tmpl::operator++(): "
                                                 "attempt to incerment end()");
      while ((++_iter != _end) && *_iter == NULL);
      return *this; 
    }
    // movement back (no post-decrement)
    gmap_iter_tmpl& operator--(void) {   
      if (_iter == _begin) throw std::out_of_range("gmap_iter_tmpl::operator--(): "
                                                   "attempt to decrerment begin()");
      while ((--_iter != _begin) && *_iter == NULL);
      return *this;
    }
    // pointed group ID
    GID operator*(void) const { 
      if (_iter == _end) throw std::out_of_range("gmap_iter_tmpl::operator*(): "
                                                 "attempt to dereference end()");
      return _iter - _begin;
    }
    // reference to the group content
    Cont& gcontent(void) {
      if (_iter == _end) throw std::out_of_range("gmap_iter_tmpl::gcontent(): "
                                                 "attempt to get clauses from end()");
      return (*_iter)->content;
    }
    // this is for backward-compatibility and consistency with main interface
    Cont& gclauses(void) { return gcontent(); } 
    Cont& vgvars(void) { return gcontent(); }
    // reference to the size of active (i.e. non-removed) content
    ULINT& a_count(void) {
      if (_iter == _end) throw std::out_of_range("gmap_iter_tmpl::a_count(): "
                                                 "attempt to get counts from end()");
      return (*_iter)->a_count;
    }

  private:
    GM_iter _iter;      // the underlying iterator
    GM_iter _begin;     // the begining (for range checks)
    GM_iter _end;       // the end (for range checks)
  };


public:         // Lifecycle

  /* Default contructor - does not try to optimize, no occs_list */
  BasicGroupSet(void) 
    : _gmap((size_t)1, NULL) {}

  /* Looks into configuration settings, and sets various optimization parameters
   * based on the configuration */
  BasicGroupSet(ToolConfig& config) 
    : _gmap((size_t)1, NULL) {
    // mode: CNF or GCNF
    _mode = (config.get_grp_mode() ? 2 : 1);
    // occs list is needed for BCP, BCE and for model rotation
    _poccs_list = 
      (config.get_model_rotate_mode() 
       || config.get_bcp_mode() 
       || config.get_bce_mode()
       || config.get_ve_mode()
       || config.get_var_mode()) ? new OccsList() : NULL;
    // units are needed for BCP and VE
    _store_units = config.get_bcp_mode() || config.get_ve_mode();
    // variable maps needed for the VMUS (and similar) modes
    _make_vgmap = (!config.get_var_mode()) ? 0 : (config.get_grp_mode() ? 2 : 1);
  }    

  virtual ~BasicGroupSet(void) {
    // free the clause vectors - clauses themselves are taken care of by the
    // clause manager
    for (GIDMap::iterator pgi = _gmap.begin(); pgi != _gmap.end(); ++pgi)
      if (*pgi != NULL)
        delete *pgi;
    if (_poccs_list != NULL)
      delete _poccs_list;
  }

  /* Allows to clear the group-set; preserves the mode, whether or not to
   * store units; and whether or not to make vgmap 
   */
  void clear(void) {
    _clvec.clear();
    for (GIDMap::iterator pgi = _gmap.begin(); pgi != _gmap.end(); ++pgi)
      if (*pgi != NULL)
        delete *pgi;
    _gmap.clear();
    _max_gid = 0;
    _max_var = 0;
    _max_id = 0;
    _size = _init_size = 0;
    _gsize = _init_gsize = 0;
    // _mode = 0; keep the mode
    if (_poccs_list != NULL) {
      delete _poccs_list;
      _poccs_list = new OccsList();
    }
    // _store_units = false; keep this flag also
    _units.clear();
    _empty = 0;
    // _make_vgmap = 0; keep the mode
    _vgmap.clear();
    _rvgmap.clear();
    _max_vgid = 0;
    _vgsize = 0;
    _vsize = 0;
  }
  
public:    // Info

  /* Maximum variable ID in the groupset */
  ULINT max_var(void) const { return _max_var; }

  /* Returns the maximum among group IDs */
  GID max_gid(void) const { return _max_gid; }

  /* Returns the maximum among variable group IDs */
  GID max_vgid(void) const { return _max_vgid; }

  /* Returns the maximum clause ID */
  ULINT max_id(void) const { return _max_id; }

  /* Returns the number of clauses */
  ULINT size(void) const { return _size; }

  /* Returns the number of groups */
  ULINT gsize(void) const { return _gsize; }

  /* Returns the number of variable groups (>= 1; 0 if not in var mode) */
  ULINT vgsize(void) const { return _vgsize; }

  /* Returns the actual number of variables (0 if not in var mode) */
  ULINT vsize(void) const { return _vsize; }

  /* Stores/retrieves initial sizes (for convenience only) */
  void set_init_size(ULINT init_size) { _init_size = init_size; }
  ULINT init_size(void) const { return _init_size; }
  void set_init_gsize(ULINT init_gsize) { _init_gsize = init_gsize; }
  ULINT init_gsize(void) const { return _init_gsize; }

public:    // Creation and addition of clauses (note that create_clause is used
           // by parsers, so keep it consistent)
  
  /* Makes a "disconnected" clause from the given vector of literals. The
   * clause gets unique automatic clause ID (if clid != 0), the literals get 
   * sorted, dublicates are removed, but the clause is not added to the group   
   * set. Note that the vector of literals 'clits' gets sorted and cleaned-up 
   * as a side effect.
   */
  BasicClause* make_clause(vector<LINT>& clits, LINT clid = 0) {
    sort(clits.begin(), clits.end(), AbsLitLess());
    _clreg.remove_duplicates(clits);
    BasicClause *cl = new BasicClause(clits);
    cl->mark_sorted();
    if (clid != 0)
      cl->set_id(clid);
    return cl;  
  }

  /* Returns true if the clause with the same literals is already in the 
   * groupset
   */
  bool exists_clause(BasicClause* cl) { 
    return _clreg.lookup_vect(cl->cl_lits()) != NULL;
  }

  /* Returns a pointer to the clause with the same literals or NULL if not
   * there
   */
  BasicClause* lookup_clause(BasicClause* cl) { 
    return _clreg.lookup_vect(cl->cl_lits());
  }

  /* Adds a clause to the group set. If the clause with the same literals
   * is already in the group set, it is returned instead (note that it
   * may have a different clause id, and already set group ID). Otherwise,
   * the clause is added to the group set datastructures, including occlists.
   * The clause expected to have undefined group ID. The clause itself is
   * returned in this case (i.e. the return value can be used to know if a 
   * new clause is added).
   */
  BasicClause* add_clause(BasicClause* cl) {
    assert(cl->get_grp_id() == gid_Undef);
    assert(!cl->unsorted());
    BasicClause* res = _clreg.lookup_vect(cl->cl_lits());
    if (res != NULL) // already there
      return res;
    // new clause
    _clvec.push_back(cl);
    _size++;
    if (cl->size()) {
      // update max var
      ULINT last_var = abs(*(cl->begin() + cl->size() - 1)); // sorted
      if (_max_var < last_var) {
        _max_var = last_var;
        if (has_occs_list())
          _poccs_list->resize(_max_var);
      }
      // if default _vgmap is needed, add the variables of the clause to the 
      // _vgmap; for the default map VGID = variable id
      if (_make_vgmap == 1) {
        if (_max_vgid < _max_var) {
          _max_vgid = _max_var;
          _vgmap.resize(_max_vgid + 1, 0);
          _rvgmap.resize(_max_var + 1, gid_Undef);
        }
        for (CLiterator plit = cl->abegin(); plit != cl->aend(); ++plit) {
          ULINT var = abs(*plit);
          if (_vgmap[var] == NULL) {
            VGroupInfo* vgi = new VGroupInfo();
            vgi->content.push_back(var);
            vgi->a_count = 1;
            _vgmap[var] = vgi;
            _rvgmap[var] = (GID)var; // default group ID = variable ID
            _vgsize++; // new group
            _vsize++; // new var
          }
        }
      }
    } else {
      _empty = cl;
    }
    if (cl->get_id() > _max_id) // update max_id
      _max_id = cl->get_id(); 
    if (has_occs_list()) {         // update the list 
      for (Literator lpos = cl->begin(); lpos != cl->end(); ++lpos) {
        _poccs_list->clauses(*lpos).push_back(cl);
        ++(_poccs_list->active_size(*lpos));
      }
    }
    if (_store_units && cl->size() == 1) // remember units
      _units.push_back(cl);
    _clreg.register_clause(cl);
    return cl;
  }

  /* Creates and inserts a new clause into group set; when clid != 0, the
   * clause id will be set to be clid. If an identical clause has already 
   * been added its returned instead, clid is always ignored in this case.
   */
  BasicClause* create_clause(vector<LINT>& clits, LINT clid = 0) {
    return add_clause(make_clause(clits, clid));
  }

  /* Sets the group id, and inserts the clause into group map 
   * TODO: should we allow same clause in the different group ? 
   */
  void set_cl_grp_id(BasicClause* ncl, GID gid) {
    // if the clause already has gid, assume that we've already added it
    if (ncl->get_grp_id() == gid_Undef) {
      ncl->set_grp_id(gid);
      if (_max_gid < gid) {
        _max_gid = gid;
        _gmap.resize(gid+1, NULL);
      }
      GroupInfo*& gi = _gmap.at(gid);
      if (gi == NULL) { // new group
        gi = new GroupInfo();
        _gsize++;
      }
      gi->content.push_back(ncl);
      gi->a_count++;
    }
  }

public:         // Presudo-removal of clauses and groups 

  /* Pseudo-removes the group from the groupset -- i.e. pseudo-removes all
   * of its clauses (see below).
   */
  void remove_group(GID gid) {
    // mark the clauses as removed (and update counts in the occlist)
    BasicClauseVector& clv = gclauses(gid);
    for (auto cl : clv)
      if (!cl->removed())
        remove_clause(cl);
  }

  /* Pseudo-removes the clause from the groupset: the clause is marked removed, 
   * but left to hang around, because we might need it later to re-build the 
   * solution. If the occlists are maintained, the active counts for its 
   * literals in the occs list are decremented, and the active count for its 
   * group is decremented. Note that the clause is not physically removed from 
   * the occs list as it would be too expensive, instead the occs lists are 
   * cleaned up lazily.
   */
  void remove_clause(BasicClause* cl) 
  {
    assert(!cl->removed());
    cl->mark_removed();
    if (cl->asize() == 0)
      _empty = NULL;
    if (has_occs_list()) {
      _poccs_list->update_active_sizes(cl);
      --a_count(cl->get_grp_id());
    }
    if (cl->asize() < cl->size()) {
      cl->restore();
      cl->sort_alits();
    }
  }

  /* Simply frees the memory for the clause: this is the inverse of make_clause */
  void destroy_clause(BasicClause* cl) { delete cl; }

public:    // Access to clauses

  /* Iterator to the first clause */
  cvec_iterator begin(void) { return _clvec.begin(); }
  cvec_citerator begin(void) const { return _clvec.begin(); }

  /* Iterator past the last clause */
  cvec_iterator end(void) { return _clvec.end(); }
  cvec_citerator end(void) const { return _clvec.end(); }

public:    // Access to non-empty groups of clauses - via custom iterator

  typedef gmap_iter_tmpl<GIDMap, BasicClauseVector> gset_iterator;

  /* Iterator to the first non-empty group */
  gset_iterator gbegin(void) { return gset_iterator(_gmap, _gmap.begin()); }

  /* Iterator past the last non-empty group */
  gset_iterator gend(void) { return gset_iterator(_gmap, _gmap.end()); }

  /* True if the group is a non-empty group in the group set
   */
  bool gexists(GID gid) const { return (gid <= _max_gid) && (_gmap[gid] != NULL); }

  /* Returns true if the group set has g0 */
  bool has_g0(void) const { return gexists(0); }

  /* Returns a reference to the vector of clauses for the specified group ID, 
   * throws std::out_of_range if group ID does not exist
   */
  BasicClauseVector& gclauses(GID gid) {
    GroupInfo* pgi = NULL;
    if ((gid > _max_gid) || ((pgi = _gmap[gid]) == NULL))
      throw std::out_of_range("non-existent group");
    return pgi->content;
  }
  const BasicClauseVector& gclauses(GID gid) const { 
    return const_cast<BasicGroupSet*>(this)->gclauses(gid); 
  }

  /* Returns a reference to the count of active clauses for the specified group 
   * ID, throws std::out_of_range if group ID does not exist
   */
  ULINT& a_count(GID gid) {
    GroupInfo* pgi = NULL;
    if ((gid > _max_gid) || ((pgi = _gmap[gid]) == NULL))
      throw std::out_of_range("non-existent group");
    return pgi->a_count;
  }
  const ULINT& a_count(GID gid) const { 
    return const_cast<BasicGroupSet*>(this)->a_count(gid); 
  }

public:  // Access to occurences lists

  bool has_occs_list(void) const { return _poccs_list != NULL; }
  OccsList& occs_list(void) { return *_poccs_list; }
  const OccsList& occs_list(void) const { return *_poccs_list; }
  void drop_occs_list(void) { 
    if (has_occs_list()) { 
      delete _poccs_list; _poccs_list = NULL; 
    }
  }

public:    // Access to non-empty groups of variables

  /* Sets the group id of variable -- this only supposed to happen in group 
   * variable mode. TODO: should we allow same variable in different group ? 
   */
  void set_var_grp_id(ULINT var, GID vgid) {
    // make it a hard error for now
    if (_make_vgmap != 2) 
      throw std::logic_error("BasicGroupSet::set_var_grp_id() is used inappropriately.");
    if (_max_vgid <= vgid) {
      _max_vgid = vgid;
      _vgmap.resize(_max_vgid + 1, NULL);
    }
    if (_vgmap[vgid] == NULL) {
      _vgmap[vgid] = new VGroupInfo();
      _vgsize++; // new group
    }
    _vgmap[vgid]->content.push_back(var);
    _vgmap[vgid]->a_count++;
    if (_rvgmap.size() < var + 1)
      _rvgmap.resize(std::max(var,_max_var) + 1, gid_Undef);
    if (_rvgmap[var] == gid_Undef)
      _vsize++; // new var
    _rvgmap[var] = vgid;
  }

  /* Returns the variable group ID of variable 
   */
  GID get_var_grp_id(ULINT var) { assert(_make_vgmap >= 1); return _rvgmap[var]; }

  typedef gmap_iter_tmpl<VGIDMap, VarVector> vgset_iterator;

  /* Iterator to the first non-empty group */
  vgset_iterator vgbegin(void) { return vgset_iterator(_vgmap, _vgmap.begin()); }

  /* Iterator past the last non-empty group */
  vgset_iterator vgend(void) { return vgset_iterator(_vgmap, _vgmap.end()); }

  /* True if the group is a non-empty group in the group set
   */
  bool vgexists(GID vgid) const { 
    return (vgid <= _max_vgid) && (_vgmap[vgid] != NULL); 
  }

  /* Returns a reference to the vector of variables for the specified group ID, 
   * throws std::out_of_range if group ID does not exist
   */
  VarVector& vgvars(GID vgid) {
    VGroupInfo* pgi = NULL;
    if ((vgid > _max_vgid) || ((pgi = _vgmap[vgid]) == NULL))
      throw std::out_of_range("non-existent group");
    return pgi->content;
  }
  const VarVector& vgvars(GID vgid) const { 
    return const_cast<BasicGroupSet*>(this)->vgvars(vgid); 
  }

public:  // Access to unit and empty clauses

  BasicClauseVector& units(void) { return _units; }
  const BasicClauseVector& units(void) const { return _units; }

  BasicClause* empty_clause(void) const { return _empty; }
  bool has_empty(void) const { return _empty != NULL; }

public:  // Methods called from parsers to pass on the info from input files

  void set_num_vars(ULINT nvars) { 
    if (has_occs_list())
      _poccs_list->init(nvars);
    if (_make_vgmap) 
      _rvgmap.reserve(nvars+1);
  }

  void set_num_cls(ULINT ncls) {        // reserve space
    if (_mode) {
      _clvec.reserve(ncls);
      if (_mode == 1)
        _gmap.reserve(ncls+1);
    }
  }      

  void set_num_grp(XLINT ngrp) {        // reserve space
    if (_mode == 2)
      _gmap.reserve(ngrp+1);
  }
  
  void set_num_vgrp(ULINT nvgrp) {       // reserve space
    if (_make_vgmap)
      _vgmap.reserve(nvgrp+1);
  }

public:    // Output functions

  void dump(std::ostream& out=std::cout) const {
    BasicGroupSet* ths = const_cast<BasicGroupSet*>(this);
    for (gset_iterator pg = ths->gbegin(); pg != ths->gend(); ++pg) {
      BasicClauseVector& cls = pg.gclauses();
      for (ClVectIterator pc = cls.begin(); pc != cls.end(); ++pc) {
        out << "[gid=" << *pg << "] "; (*pc)->dump(out); out << std::endl;
      }
    }
    if (has_occs_list()) {
      out << std::endl;
      _poccs_list->dump(out);
    }
    if (_make_vgmap) {
      out << "Variable group map:" << std::endl;
      for (vgset_iterator pvg = ths->vgbegin(); pvg != ths->vgend(); ++pvg) {
        out << "[vgid=" << *pvg << "]: vars={ ";
        copy(pvg.vgvars().begin(), pvg.vgvars().end(), 
             std::ostream_iterator<ULINT, char>(out, " "));
        out << "}" << std::endl;
      }
    }
  }

  friend std::ostream& operator<< (std::ostream& out, const BasicGroupSet& gs) {
    gs.dump(out);
    return out;
  }


public:    // Additions for the proof-checker instances

  ULINT get_first_abbr(void) const { return _first_abbr; }
  void set_first_abbr(ULINT fa) { _first_abbr = fa; }

  ULINT get_first_sel(void) const { return _first_sel; }
  void set_first_sel(ULINT fs) { _first_sel = fs; }

protected:

  ClauseRegistry _clreg;   // clause registry (personal copy) 

  BasicClauseVector _clvec;// vector of all clauses

  GIDMap _gmap;            // index = GID, value = GroupInfo (vector of clauses + ...)

  GID _max_gid = 0;        // maximum GID

  ULINT _max_var = 0;      // maximum variable ID

  ULINT _max_id = 0;       // maximum clause ID

  ULINT _size = 0;         // number of clauses
  ULINT _init_size = 0;    // -"- initial

  ULINT _gsize = 0;        // number of groups
  ULINT _init_gsize = 0;   // -"- initial

  int _mode = 0;           // used for optimizations: 0 = unknown, 1 = CNF, 2 = GCNF

  OccsList* _poccs_list = 0;// created and populated if needed

  bool _store_units = false;// if true, makes a list of unit clauses

  BasicClauseVector _units;// the list of unit clauses

  BasicClause* _empty;     // empty clause (if there)

  unsigned _make_vgmap = 0;// 0 - no map, 1 - default map (i.e. vgid = var id), 
                           // 2 - custom map

  VGIDMap _vgmap;          // index = GID, value = vector of vars

  GIDVector _rvgmap;       // reverse map: index = var, value = GID

  GID _max_vgid = 0;       // maximum variable group GID

  ULINT _vgsize = 0;       // number of variable groups

  ULINT _vsize = 0;        // number of variables

  ULINT _first_abbr = 0;    // for proof-checker based instances

  ULINT _first_sel = 0;    // for proof-checker based instances
};


/** Iterator typedefs -- for backward compatibility and convenience
 */
typedef BasicGroupSet::gset_iterator gset_iterator;     // groups of clauses
typedef BasicGroupSet::vgset_iterator vgset_iterator;   // groups of variables

#endif /* _BASIC_GROUP_SET_H */

/*----------------------------------------------------------------------------*/
