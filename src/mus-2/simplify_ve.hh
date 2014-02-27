/*----------------------------------------------------------------------------*\
 * File:        simplify_ve.hh
 *
 * Description: Class definition and implementation of a work item for VE- 
 *              based (SatElite) simplification.
 *
 * Author:      antonb
 *
 * Notes:
 *      1.      See ve_simplifier.cc for (a lot more) of the details.
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _SIMPLIFY_VE_HH
#define _SIMPLIFY_VE_HH 1

#include <ext/hash_map>
#include <vector>
#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  SimplifyVE
 *
 * Purpose: A work item for BCP-based simplification of group set. 
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class SimplifyVE : public WorkItem {

public:     // Lifecycle

  SimplifyVE(MUSData& md, bool g_mode = true)          
    : _md(md), _g_mode(g_mode), _res_lim(20), _sub_lim(1000), 
      _confl(NULL), _cpu_time(0), _rcl_count(0), _rg_count(0) {}

  virtual ~SimplifyVE(void) {}

public:     // Parameters

  MUSData& md(void) const { return _md; }

  /* Mode: group or not */
  bool group_mode(void) const { return _g_mode; }
  void set_group_mode(bool g_mode) { _g_mode = g_mode; }

  /* Resolvent limit -- elimination is not performed if it produces a 
   * resolvent longer than this. <0 means "no limit". Courtesy of minisat.
   */
  int res_lim(void) const { return _res_lim; }

  /* Subsumption length limit -- clauses over this length are not considered
   * during backward subsumption check. <0 means "no limit". Courtesy of minisat.
   */
  int sub_lim(void) const { return _sub_lim; }

public:     // Results

  /* Returns the version of MUSData the results are for */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

  /* Conflict status and info */
  bool conflict(void) const { return _confl != NULL; }
  void set_conflict_clause(BasicClause* confl) { _confl = confl; }
  BasicClause* conflict_clause(void) { return _confl; }
  const BasicClause* conflict_clause(void) const { return _confl; }

  /** Resolution derivation data -- used for reconstruction of solution */
  struct ResData {
    BasicClause* r1;    // resolvent 1
    BasicClause* r2;    // resolvent 2
    ULINT v;            // var
    unsigned count;     // number of candidates
    ResData(BasicClause* rr1 = NULL, BasicClause* rr2 = NULL, ULINT vv = 0, 
            unsigned cc = 0)
      : r1(rr1), r2(rr2), v(vv), count(cc) {}
  };
  typedef __gnu_cxx::hash_map<BasicClause*, ResData, ClPtrHash, ClPtrEqual> DerivData;
  /* Returns the instance of the derivation data */
  const DerivData& dd(void) const { return _dd; }
  DerivData& dd(void) { return _dd; }

  /** Elimination trace -- most recent last (TODO: remove) */
  const std::vector<ULINT>& trace(void) const { return _trace; }
  std::vector<ULINT>& trace(void) { return _trace; }

public:     // Stats (r/w)

  /* Elapsed CPU time (seconds) */
  double& cpu_time(void) { return _cpu_time; }
  double cpu_time(void) const { return _cpu_time; }

  /* The number of removed clauses */
  unsigned& rcl_count(void) { return _rcl_count; }
  unsigned rcl_count(void) const { return _rcl_count; }

  /* The number of fully removed groups */
  unsigned& rg_count(void) { return _rg_count; }
  unsigned rg_count(void) const { return _rg_count; }

public:     // Reset/recycle

  virtual void reset(void) {
    _confl = NULL;
    _dd.clear();
    _cpu_time = 0;
    _rcl_count = 0;
    _rg_count = 0;
  }

protected:

  // parameters

  MUSData& _md;             // MUS data

  bool _g_mode;             // true for group mode

  int _res_lim;             // limit on the length of the resolvent

  int _sub_lim;             // limit on the length for subs. check

  // results

  unsigned _version;        // the version of MUSData this result is for

  bool _conflict;           // true when a coflict is detected in BCP

  BasicClause* _confl;      // conflict clause (if non-NULL)

  DerivData _dd;            // derivation information

  std::vector<ULINT> _trace;// elimination trace - most recent last (TODO: remove)

  // statistics 

  double _cpu_time;         // CPU time (seconds) used for simplification

  unsigned _rcl_count;      // number of removed clauses

  unsigned _rg_count;       // number of fully removed groups

};

#endif /* _SIMPLIFY_VE_HH */

/*----------------------------------------------------------------------------*/
