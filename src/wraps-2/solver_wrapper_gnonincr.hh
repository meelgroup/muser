/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gnonincr.hh
 *
 * Description: Base class for non-incremental group-based wrappers.
 *
 * Author:      antonb 
 *
 * Notes:
 *   1. concrete classes need to implement solver_add_clause(), plus all the 
 *   extra operations in the main interface methods; the SAT solver underneath
 *   needs to know to compute UNSAT core.
 *
 *   2. *NOT TESTED*
 *
 *                        Copyright (c) 2011-12, Anton Belov, Joao Marques-Silva
\*----------------------------------------------------------------------------*/

#ifndef _SOLVER_WRAPPER_GNONINCR_H
#define _SOLVER_WRAPPER_GNONINCR_H 1

#include "solver_wrapper.hh"

/*----------------------------------------------------------------------------*\
 * Class: SATSolverWrapperGrpNonincr
 *
 * Purpose: ABC for incremental group-based wrappers.
 *
\*----------------------------------------------------------------------------*/

class SATSolverWrapperGrpNonIncr : public SATSolverWrapper {

  friend class SATSolverFactory;

protected:  // Constructor/destructor

  SATSolverWrapperGrpNonIncr(IDManager& _imgr)
    : SATSolverWrapper(_imgr) {}
  
  virtual ~SATSolverWrapperGrpNonIncr(void) {}

public: // Implementation of the main interface methods

  /* Initialize all internal data structures */
  virtual void init_all(void) {
    model.clear(); gcore.clear(); gid2st_map.clear(); local_gset.clear();
    maxvid = 0; maxgid = 0; isvalid = false;
  }

  /* Clean up all internal data structures */
  virtual void reset_all(void) {
    model.clear(); gcore.clear(); gid2st_map.clear(); local_gset.clear(); 
    isvalid = false;
  }

  /* Initialize data structures for SAT run */
  virtual void init_run(void) {
    if (isvalid)
      throw std::logic_error("Solver interface is in invalid state.");
    model.clear(); gcore.clear(); isvalid = true;
  }

  /* Clean up data structures from SAT run */
  virtual void reset_run(void) {
    if (!isvalid)
      throw std::logic_error("Solver interface is in invalid state.");
    model.clear(); gcore.clear(); isvalid = false;
  }

  /* Solve the current set of clauses instance */
  virtual SATRes solve(void) = 0;

  /* Solve the current set of clauses with extra assumptions;
   * assumptions are given as units in 'assum' and passed directly
   * to the solver without any modifications.
   */
  virtual SATRes solve(const IntVector& assum) = 0;


public: // Configuration methods

  /* Verbosity -- usually passed to solver */
  void set_verbosity(int verb) = 0;

  /* Sets the default phase: 0 - false, 1 - true, 2 - random */
  void set_phase(LINT ph) = 0; 


public:   // Access result of SAT solver call

  /* Returns the reference to the group unsat core */
  virtual GIDSet& get_group_unsat_core(void) { return gcore; } 

public: // Implemented group interface

  /* Number of groups (including 0) */
  virtual LINT gsize(void) { return gid2st_map.size(); }

  /* Maximum GID ever used in the solver */
  virtual GID max_gid(void) { return maxgid; }

  /* Adds all groups in the groupset; g0final is ignored; makes a local
   * copy of the group set
   */
  virtual void add_groups(BasicGroupSet& gset, bool g0final = true) {
    for (gset_iterator pg = gset.gbegin(); pg != gset.gend(); ++pg) {
      GID gid = *pg;
      // if the group is completely empty
      if (!gset.a_count(gid))
        continue;
      gid2st_map.insert(make_pair(gid, -1)); // active
      BasicClauseVector& clauses = pg.gclauses();
      for (ClVectIterator pcl = clauses.begin(); pcl != clauses.end(); ++pcl) {
        BasicClause* cl = *pcl;
        if (!cl->removed()) {
          GID gid = cl->get_grp_id();
          cl->set_grp_id(gid_Undef);
          local_gset.add_clause(cl);
          local_gset.set_cl_grp_id(cl, gid);
        }
      }
    }
    update_maxgid(gset.max_gid());
    update_maxvid(gset.max_var());
  }

  /* True if group exists in the solver */
  virtual bool exists_group(GID gid) { 
    return (gid2st_map.find(gid) != gid2st_map.end()); 
  }

  /* Activates (non-final) group */
  virtual void activate_group(GID gid) {
    GID2IntMap::iterator p = gid2st_map.find(gid);
    assert(p != gid2st_map.end()); // exists
    assert(p->second == 1); // not final, not active
    p->second = -1;
  }

  /* Deactivates (non-final) group */
  virtual void deactivate_group(GID gid) {
    GID2IntMap::iterator p = gid2st_map.find(gid);
    assert(p != gid2st_map.end()); // exists
    assert(p->second == -1); // not final, active
    p->second = 1;
  }

  /* Returns true if either final, or non-final and active */
  virtual bool is_group_active(GID gid) {
    GID2IntMap::iterator p = gid2st_map.find(gid);
    assert(p != gid2st_map.end()); // exists
    return (p->second <= 0);
  }

  /* Removes (non-final) group */
  virtual void del_group(GID gid) {
    GID2IntMap::iterator p = gid2st_map.find(gid);
    assert(p != gid2st_map.end()); // exists
    gid2st_map.erase(p);
  }

  /* Finalizes a group */
  virtual void make_group_final(GID gid) {
    GID2IntMap::iterator p = gid2st_map.find(gid);
    assert(p != gid2st_map.end()); // exists
    p->second = 0;
  }

  /* True if group is final */
  virtual bool is_group_final(GID gid) {
    GID2IntMap::iterator p = gid2st_map.find(gid);
    assert(p != gid2st_map.end()); // exists
    return p->second == 0;
  }

protected: // These are the solver-specific functions -- to be implemented 
           // by subclassses

  /* Add a clause to solver, with associated assumption (0 means no assumption)
   */
  virtual void solver_add_clause(BasicClause* cl) = 0;

protected: // Helpers

  void update_maxvid(LINT nvid) {
    ULINT unvid = (nvid > 0) ? nvid : -nvid;
    if (unvid > maxvid)
      maxvid = unvid;
  }

  void update_maxgid(GID gid) {
    if (gid > maxgid)
      maxgid = gid;
  }
  
protected:

  GID2IntMap gid2st_map;            // map from group IDs to status: 
                                    // 0 - group is final; -1 - group is active; 
                                    // 1 - group is inactive

  GIDSet gcore;                     // core

  bool isvalid = false;

  ULINT maxvid = 0;                 // max variable id ever used
  
  GID maxgid = 0;                   // maximum group id (ever used)

};

#endif /* _SOLVER_WRAPPER_GNONINCR_H */

/*----------------------------------------------------------------------------*/
