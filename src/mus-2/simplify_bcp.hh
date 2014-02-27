/*----------------------------------------------------------------------------*\
 * File:        simplify_bcp.hh
 *
 * Description: Class definition and implementation of a work item for BCP- 
 *              based simplification.
 *
 * Author:      antonb
 *
 * Notes:
 *
 *                                              Copyright (c) 2011, Anton Belov
\*----------------------------------------------------------------------------*/

#ifndef _SIMPLIFY_BCP_HH
#define _SIMPLIFY_BCP_HH 1

#include "mus_data.hh"
#include "work_item.hh"

/*----------------------------------------------------------------------------*\
 * Class:  SimplifyBCP
 *
 * Purpose: A work item for BCP-based simplification of group set. 
 *
 * Notes:
 *
\*----------------------------------------------------------------------------*/

class SimplifyBCP : public WorkItem {

public:     // Lifecycle

  SimplifyBCP(MUSData& md, bool g_mode = true)          
    : _md(md), _g_mode(g_mode), _confl(NULL), _vd(md.gset().max_var()+1),
      _cpu_time(0), _rcl_count(0), _rg_count(0), _ua_count(0) {}

  virtual ~SimplifyBCP(void) {}

public:     // Parameters

  MUSData& md(void) const { return _md; }

  /* Mode: in group-mode, only g0 is simplified; in non-group mode everything */
  bool group_mode(void) const { return _g_mode; }
  void set_group_mode(bool g_mode) { _g_mode = g_mode; }

public:     // Results

  /* Returns the version of MUSData the results are for */
  const unsigned& version(void) const { return _version; }
  void set_version(unsigned version) { _version = version; }

  /* Conflict status and info */
  bool conflict(void) const { return _confl != NULL; }
  void set_conflict_clause(BasicClause* confl) { _confl = confl; }
  BasicClause* conflict_clause(void) { return _confl; }
  const BasicClause* conflict_clause(void) const { return _confl; }

  /* Variable data calculated during propagation; TODO: make into class */
  struct VarData {
    int value;                  // assigned value -1,+1,0
    BasicClause* reason;        // reason for assignment
    VarData(void) : value(0), reason(NULL) {}
  };
  VarData& var_data(ULINT var) { return _vd[var]; }

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

  /* The number of unit assignments */
  unsigned& ua_count(void) { return _ua_count; }
  unsigned ua_count(void) const { return _ua_count; }

public:     // Reset/recycle

  virtual void reset(void) {
    _confl = NULL;
    _vd.assign(_vd.size(), VarData());
    _cpu_time = 0;
    _rcl_count = 0;
    _rg_count = 0;
    _ua_count = 0;
  }

protected:

  // parameters

  MUSData& _md;             // MUS data

  bool _g_mode;             // true for group mode

  // results

  unsigned _version;        // the version of MUSData this result is for

  bool _conflict;           // true when a coflict is detected in BCP

  BasicClause* _confl;      // conflict clause (if non-NULL)

  std::vector<VarData> _vd; // index = variable_id
  
  // statistics 

  double _cpu_time;         // CPU time (seconds) used for simplification

  unsigned _rcl_count;      // number of removed clauses

  unsigned _rg_count;       // number of fully removed groups

  unsigned _ua_count;       // number of unit assignments

};

#endif /* _SIMPLIFY_BCP_HH */

/*----------------------------------------------------------------------------*/
