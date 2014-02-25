//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gincr.hh
 *
 * Description: Base class for incremental group-based wrappers.
 *
 * Author:      antonb 
 *
 * Notes:
 *
 *              - concrete classes need to implement solver_add_clause() and
 *                solver_assert_unit_clause(), plus all the extra operations
 *                in the main interface methods
 *
 *
 *                        Copyright (c) 2011-12, Anton Belov, Joao Marques-Silva
\*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _SOLVER_WRAPPER_GINCR_H
#define _SOLVER_WRAPPER_GINCR_H 1

#include "solver_wrapper.hh"

/*----------------------------------------------------------------------------*\
 * Class: SATSolverWrapperGrpIncr
 *
 * Purpose: ABC for incremental group-based wrappers.
 *
\*----------------------------------------------------------------------------*/

class SATSolverWrapperGrpIncr : public SATSolverWrapper {

  friend class SATSolverFactory;

protected:  // Constructor/destructor

  SATSolverWrapperGrpIncr(IDManager& _imgr)
    : SATSolverWrapper(_imgr) {}
  
  virtual ~SATSolverWrapperGrpIncr(void) {}

public: // Implementation of the main interface methods

  /* Initialize all internal data structures */
  virtual void init_all(void) {
    model.clear(); gcore.clear(); gid2a_map.clear(); 
    maxvid = 0; maxgid = 0; isvalid = false;
  }

  /* Clean up all internal data structures */
  virtual void reset_all(void) {
    model.clear(); gcore.clear(); gid2a_map.clear(); isvalid = false;
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


public: // Implemented non-group interface

  /* This assumes that the GID of the clause is set correctly. It will create
   * a new assumption variable if the GID has not been yet seen. This method 
   * will  not check if the clause is marked as removed. Group 0 clauses are 
   * added as final.
   */
  virtual void add_clause(BasicClause* cl) {
    GID gid = cl->get_grp_id();
    LINT aid = 0; // assumption lit
    if (gid) {
      // make new indicator, if needed
      GID2IntMap::iterator p = gid2a_map.find(gid);
      if (p != gid2a_map.end())
        aid = p->second;
      else {
        aid = imgr.new_id();
        gid2a_map.insert(make_pair(gid, -aid));
      }
    }
    solver_add_clause(cl, aid);
    update_maxgid(gid);
  }

  /* Adds a final clause (non-removable) */
  virtual void add_final_clause(BasicClause* cl) { solver_add_clause(cl); }

  /* Adds a final unit clause (non-removable) */
  virtual void add_final_unit_clause(LINT lit) { solver_assert_unit_clause(lit); } 

public: // Implemented group interface

  /* Number of groups (including 0) */
  virtual LINT gsize(void) { return gid2a_map.size() + has_g0; }

  /* Maximum GID ever used in the solver */
  virtual GID max_gid(void) { return maxgid; }

  /* Adds all groups in the groupset */
  virtual void add_groups(BasicGroupSet& gset, bool g0final = true) {
    for (gset_iterator pg = gset.gbegin(); pg != gset.gend(); ++pg)
      add_group(gset, *pg, (*pg == 0) && g0final);
    update_maxgid(gset.max_gid());
    update_maxvid(gset.max_var());
  }

  /* Adds a single group from the groupset; if final = true the group is
   * added as final right away.
   */
  virtual void add_group(BasicGroupSet& gset, GID gid, bool final = false) {
    if (gset.a_count(gid)) {
      LINT aid = 0; // assumption lit
      if (!final)
        aid = imgr.new_id();
      bool added = false;
      for (auto cl : gset.gclauses(gid))
        if (!cl->removed()) {
          solver_add_clause(cl, aid);
          added = true;
        }
      if (aid && added) 
        gid2a_map.insert(make_pair(gid, -aid));
      update_maxgid(gid);
    }
    if (!gid && final)
      has_g0 = true;
  }

  /* True if group exists in the solver */
  virtual bool exists_group(GID gid) { 
    return (gid2a_map.find(gid) != gid2a_map.end()); 
  }

  /* Activates (non-final) group */
  virtual void activate_group(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    assert(p->second > 0); // not final, not active
    p->second = -p->second;
  }

  /* Deactivates (non-final) group */
  virtual void deactivate_group(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    assert(p->second < 0); // not final, active
    p->second = -p->second;
  }

  /* Returns true if either final, or non-final and active */
  virtual bool is_group_active(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    return (p->second <= 0);
  }

  /* Removes (non-final) group */
  virtual void del_group(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    if (p->second)
      solver_assert_unit_clause(labs(p->second));  // assert assumption literal
    gid2a_map.erase(p);
  }

  /* Finalizes a group */
  virtual void make_group_final(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    solver_assert_unit_clause(-labs(p->second));  // Cancel assumption literal
    p->second = 0;
  }

  /* True if group is final */
  virtual bool is_group_final(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    return p->second == 0;
  }

  /* Returns the activation literal for group -- setting to true makes the
   * group inactive; 0 means the group has been finalized. */
  virtual LINT get_group_activation_lit(GID gid) {
    GID2IntMap::iterator p = gid2a_map.find(gid);
    assert(p != gid2a_map.end()); // exists
    return labs(p->second);
  }

protected: // These are the solver-specific functions -- to be implemented 
           // by subclassses

  /* Add a clause to solver, with associated assumption (0 means no assumption)
   */
  virtual void solver_add_clause(BasicClause* cl, LINT alit = 0) = 0;

  /* Add (assert) a unit clause */
  virtual void solver_assert_unit_clause(LINT lit) = 0;

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

  GID2IntMap gid2a_map;             // map from group IDs to assumptions; 0
                                    // means the group is final

  GIDSet gcore;                     // core

  bool isvalid = false;

  ULINT maxvid = 0;                 // max variable id ever used
  
  GID maxgid = 0;                   // maximum group id (ever used)

  bool has_g0 = false;              // true when g0 has been added as final
                                    // (this is not good; fix)
};

#endif /* _SOLVER_WRAPPER_GINCR_H */

/*----------------------------------------------------------------------------*/
