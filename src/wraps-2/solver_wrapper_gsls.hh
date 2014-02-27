//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        solver_wrapper_gsls.hh
 *
 * Description: An adapter class that provides an group-based interface to an
 *              instance of SATSolverSLSWrapper.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2013, Anton Belov
\*----------------------------------------------------------------------------*/
//jpms:ec

#pragma once

#include <unordered_map>
#include <unordered_set>
#include "solver_wrapper.hh"
#include "solver_sls_wrapper.hh"

/*----------------------------------------------------------------------------*\
 * Class: SATSolverWrapperGrpSLS
 *
 * Purpose: A group-based wrapper around SATSolverLowLevelSLSWrapper.
 * The wrapper fakes the incremental behaviour by reloading the relevant set of
 * clauses before invoking the underlying SAT solver.
 *
\*----------------------------------------------------------------------------*/

class SATSolverWrapperGrpSLS : public MUSer2::SATSolverWrapper {

  friend class MUSer2::SATSolverFactory;

protected:  // Constructor/destructor

  SATSolverWrapperGrpSLS(IDManager& _imgr, SATSolverSLSWrapper& _llwrap)
    : MUSer2::SATSolverWrapper(_imgr), llwrap(_llwrap) {}
  
  virtual ~SATSolverWrapperGrpSLS(void) {}

public: // Implementation of the main interface methods

  /* Initialize all internal data structures */
  virtual void init_all(void) override;

  /* Clean up all internal data structures */
  virtual void reset_all(void) override;

  /* Initialize data structures for SAT run */
  virtual void init_run(void) override;

  /* Clean up data structures from SAT run */
  virtual void reset_run(void) override;

  /* Solve the current set of clauses instance. Returns SAT_True, SAT_False, or
   * SAT_Unknown --- in the latter case an underapproximation of solution is
   * available through get_model() calls. */
  virtual SATRes solve(void) override { return solve(nullptr); }

  /* Solve the current set of clauses with extra assumptions;
   * assumptions are given as units in 'assum' and passed directly
   * to the solver without any modifications.
   */
  virtual SATRes solve(const IntVector& assum) override { return solve(&assum); }

public: // Configuration methods

  /* Verbosity -- usually passed to solver */
  virtual void set_verbosity(int verb) override { verbosity = verb; }

  /* Sets the maximum number of flips per call. -1 = no maximum.
   */
  virtual void set_max_conflicts(LINT max_conflicts) override {
    cutoff = (max_conflicts == -1) ? MAXULINT : (ULINT)max_conflicts; }

  /* Sets the timeout per call in seconds. 0 = no timeout
   */
  virtual void set_timeout(float to) override { timeout = to; }

public:   // Access result of SAT solver call

  /* Returns the reference to the model (r/w) */
  virtual IntVector& get_model(void) override { return _model; }

  /* Makes a copy of the model (resized as needed) */
  virtual void get_model(IntVector& rmodel) override { rmodel = _model; }

  /* Returns the reference to the group unsat core */
  virtual GIDSet& get_group_unsat_core(void) override { return gcore; }

public: // Implemented non-group interface

  /* This assumes that the GID of the clause is set correctly. It will create
   * a new assumption variable if the GID has not been yet seen. This method 
   * will  not check if the clause is marked as removed. Group 0 clauses are 
   * added as final.
   */
  virtual void add_clause(BasicClause* cl) override {
    solver_add_clause(cl, cl->get_grp_id() == 0);
  }

  /* Adds a final clause (non-removable) */
  virtual void add_final_clause(BasicClause* cl) override {
    solver_add_clause(cl, true);
  }

  /* Adds a final unit clause (non-removable) */
  virtual void add_final_unit_clause(LINT lit) override {
    solver_assert_unit_clause(lit);
  }

public: // Implemented group interface

  /* Number of groups (including 0) */
  virtual LINT gsize(void) override { return g2st_map.size(); }

  /* Maximum GID ever used in the solver */
  virtual GID max_gid(void) override { return maxgid; }

  /* Adds all groups in the groupset */
  virtual void add_groups(BasicGroupSet& gset, bool g0final = true) override;

  /* Adds a single group from the groupset; if final = true the group is
   * added as final right away.
   */
  virtual void add_group(BasicGroupSet& gset, GID gid, bool final = false) override;

  /* True if group exists in the solver */
  virtual bool exists_group(GID gid) override { return g2st_map.count(gid); }

  /* Activates (non-final) group */
  virtual void activate_group(GID gid) override;

  /* Deactivates (non-final) group */
  virtual void deactivate_group(GID gid) override;

  /* Returns true if either final, or non-final and active */
  virtual bool is_group_active(GID gid) override;

  /* Removes (non-final) group */
  virtual void del_group(GID gid) override;

  /* Finalizes a group */
  virtual void make_group_final(GID gid) override;

  /* True if group is final */
  virtual bool is_group_final(GID gid) override;

public:  // Miscellaneous (stats, printing, etc)

protected: // helpers

  /* Internal cleanup */
  void cleanup(void);

  /* Loads clauses in to the solver and runs the solve() */
  SATRes solve(const IntVector* assump);

  /* Adds a clause to the internal lists  */
  void solver_add_clause(BasicClause* cl, bool final = false);

  /* Add (assert) a unit clause (final) */
  void solver_assert_unit_clause(LINT lit);

  void update_maxgid(GID gid) { if (gid > maxgid) { maxgid = gid; } }
  
protected:

  bool isvalid = false;

  SATSolverSLSWrapper& llwrap;              // low-level SLS wrapper

  int verbosity = 0;                        // verbosity

  ULINT cutoff = MAXULINT;                  // max.flips per call

  float timeout = 0.0f;                     // timeout per call (0 = no timeout)

  std::unordered_map<GID,int> g2st_map;     // map from group IDs to status:
                                            // 0 - group is final;
                                            // 1 - group is active;
                                            // -1 - group is inactive

  BasicClauseSet cset;                      // all regular clauses are here

  BasicClauseSet f_cset;                    // final clauses, but with non-0 group

  std::vector<LINT> units;                  // extra units

  GIDSet gcore;                             // group core

  GID maxgid = 0;                           // maximum group id (ever used)

private:

  IntVector _clits;                         // vector for literals

  IntVector _model;                         // for to deal with const

};

/*----------------------------------------------------------------------------*/
