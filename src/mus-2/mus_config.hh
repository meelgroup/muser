//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        mus_config.hh
 *
 * Description: Configuration of MUS extractors.
 *
 * Author:      jpms, antonb
 *
 *                      Copyright (c) 2010-2012, Joao Marques-Silva, Anton Belov
 \*----------------------------------------------------------------------------*/
//jpms:ec

#ifndef _MUS_CONFIG_H
#define _MUS_CONFIG_H 1

#include "solver_config.hh"

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Types & Defines
 \*----------------------------------------------------------------------------*/
//jpms:ec

#define cfg_pref  config.get_prefix()

#define cout_pref cout << cfg_pref

#define report(x) cout << cfg_pref << x << endl;

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Purpose: Useful templates
 \*----------------------------------------------------------------------------*/
//jpms:ec

template <class T>
int getval(T id) { return (id > 0) ? 0 : 1; }


//jpms:bc
/*----------------------------------------------------------------------------*\
 * Class: ToolConfig
 *
 * Purpose: Options configuring tool execution.
 \*----------------------------------------------------------------------------*/
//jpms:ec

class ToolConfig : public SATSolverConfig {

public:

  ToolConfig() : SATSolverConfig() {}

  string& get_cmdstr() { return _cmdstr; }

  void append_cmdstr(const char* cmd) { _cmdstr += " "; _cmdstr += cmd; }

  int get_verbosity() { return _verbosity; }

  void set_verbosity(int verb) { _verbosity = verb; }

  int get_timeout() { return _timeout; }

  void set_timeout(int tout) { _timeout = tout; }

  const char* get_prefix() { return _output_prefix; }

  bool get_comp_format() { return _comp_fmt; }

  void set_comp_format() { _comp_fmt = true; _output_prefix = "c "; }

  void unset_comp_format() { _comp_fmt = false; _output_prefix = "c "; }

  void set_comp_mode() { _comp_mode = true; }

  void unset_comp_mode() { _comp_mode = false; }

  bool get_comp_mode() { return _comp_mode; }

  const char* get_output_file() { return _output_file; }

  void set_output_file(const char* ofile) { _output_file = ofile; }

  const char* get_sat_solver(void) { return _solver; } 

  bool chk_sat_solver(const char* tsolver) { return !strcmp(_solver, tsolver); }

  void set_sat_solver(const char* tsolver) { _solver = tsolver; }

  int get_phase() { return _phase; }

  void set_phase(int nph) { _phase = nph; }

  bool get_init_unsat_chk() { return _init_unsat_chk; }

  void set_init_unsat_chk() { _init_unsat_chk = true; }

  void unset_init_unsat_chk() { _init_unsat_chk = false; }

  bool get_incr_mode() { return _incr_mode; }

  void set_incr_mode() { _incr_mode = true; }

  void unset_incr_mode() { _incr_mode = false; }

  bool get_trim_mode() { return _trim_uset; }

  void set_trim_mode() { _trim_uset = true; }

  void unset_trim_mode() { _trim_uset = false; }

  ULINT get_trim_iter() { return _trim_iter; }

  void set_trim_iter(ULINT niter) {
    _trim_iter = niter;
    (niter == 0) ? unset_trim_mode() : set_trim_mode();
  }

  ULINT get_trim_percent() { return _trim_prct; }

  void set_trim_percent(ULINT nprct) {
    _trim_prct = nprct;
    (nprct == 0) ? unset_trim_mode() : set_trim_mode();
  }

  bool get_trim_fixpoint() { return _trim_fp; }

  void set_trim_fixpoint() { set_trim_mode(); _trim_fp = true; }

  void unset_trim_fixpoint() { unset_trim_mode(); _trim_fp = false; }

  void set_grp_mode() { _grp_mode = true; }

  void unset_grp_mode() { _grp_mode = false; }

  bool get_grp_mode() { return _grp_mode; }

  bool get_rm_red_mode() { return _red_mode; }

  void set_rm_red_mode() { _red_mode = true; _reda_mode = false; }

  void unset_rm_red_mode() { _red_mode = false; }

  bool get_rm_reda_mode() { return _reda_mode; }

  void set_rm_reda_mode() { _reda_mode = true; _red_mode = false; }

  void unset_rm_reda_mode() { _reda_mode = false; }

  bool get_refine_clset_mode() { return _refine_cset; }

  void set_refine_clset_mode() { _refine_cset = true; }

  void unset_refine_clset_mode() { _refine_cset = false; }

  bool get_model_rotate_mode() { return _rmr_mode; }

  void unset_model_rotate_mode() { _rmr_mode = false; }

  bool get_rmr_mode() { return _rmr_mode; }

  void set_rmr_mode() { _rmr_mode = true; }

  bool get_reorder_mode() { return _reorder_mode; }

  void set_reorder_mode() { _reorder_mode = true; }

  void unset_reorder_mode() { _reorder_mode = false; }

  bool get_mus_mode() { return _mus_mode; }

  void set_mus_mode() { _mus_mode = true; }

  void unset_mus_mode() { _mus_mode = false; }

  bool get_del_mode() { return _del_mode; }

  void set_del_mode() { 
    _del_mode = true; _ins_mode = false; _dich_mode = false; 
  }

  void unset_del_mode() { _del_mode=false; }

  bool get_ins_mode() { return _ins_mode; }

  void set_ins_mode() { 
    _ins_mode = true; _del_mode = false; _dich_mode = false;
  }

  void unset_ins_mode() { _ins_mode=false; }

  bool get_dich_mode() { return _dich_mode; }

  void set_dich_mode() { 
    _dich_mode = true; _ins_mode = false; _del_mode = false;    
  }

  void unset_dich_mode() { _ins_mode = false; }

  bool get_test_mode() { return _test_mode; }

  void set_test_mode() { _test_mode = true; }

  void unset_test_mode() { _test_mode = false; }

  unsigned get_order_mode() { return _order_mode; }

  void set_order_mode(unsigned order_mode) { _order_mode = order_mode; }

  void unset_order_mode() { _order_mode = 0; }

  void get_cfgstr(string& cfgstr) {

    if (!_incr_mode)  { cfgstr += " -noincr"; }

    if (!_mus_mode) { 
      cfgstr += " -nomus"; 
    }

    if (_ins_mode) { cfgstr += " -ins"; }

    if (_dich_mode) { cfgstr += " -dich"; }

    if (_trim_uset) {
      if (_trim_fp) {
        cfgstr += " -tfp";
      }
      else if (_trim_prct > 0) {
        cfgstr += " -tprct ";
        cfgstr += convert<int>(_trim_prct);
      }
      else if (_trim_iter > 0) {
        cfgstr += " -trim ";
        cfgstr += convert<int>(_trim_iter);
      }
      else { tool_abort("Trimming active without trim value set??"); }
    }

    if (!_refine_cset) { cfgstr += " -norf"; }

    if (chk_sat_solver("minisat")) cfgstr += " -minisat";

    if (chk_sat_solver("minisats")) cfgstr += " -minisats";

    if (_red_mode)      { cfgstr += " -rr"; }

    if (_reda_mode)      { cfgstr += " -rra"; }

    if (get_model_rotate_mode()) {
      if (_reorder_mode)
        cfgstr += " -reorder";
    } else {
      cfgstr += " -norot";
    }
    if (_order_mode) { cfgstr += " -order "; cfgstr += convert<int>(_order_mode); }

    if (_init_unsat_chk) { cfgstr += " -ichk"; }
    if (_test_mode) { cfgstr += " -test"; }

    cfgstr += " -ph ";
    cfgstr += convert<int>(_phase);

    if (_comp_fmt)   { cfgstr += " -comp"; }

    cfgstr += " -T ";
    cfgstr += convert<int>(_timeout);

    cfgstr += " -v ";
    cfgstr += convert<int>(_verbosity);
  }

protected:

  string _cmdstr;               // For printing out

  int _verbosity = 0;           // Verbosity

  int _timeout = 3600;          // TO value

  const char* _output_prefix = "c ";

  const char* _output_file = NULL;

  const char* _solver = "minisat"; // SAT solver name

  bool _comp_fmt = false;       // True for competition format

  bool _comp_mode = false;      // True for competition mode

  bool _write_mus = false;      // True if MUS needs to be dumped

  int _phase = 3;               // Phase for SAT solvers:
                                //   0 - false
                                //   1 - true
                                //   2 - random
                                //   3 - default of the SAT solver.
 
  int _incr_mode = true;        // True if incremental SAT mode

  bool _init_unsat_chk = false; // True if need to do initial UNSAT check

  bool _trim_uset = false;      // True if clset is to be iteratively trimmed

  ULINT _trim_iter = 0;         // Number of trim iterations

  ULINT _trim_prct = 0;         // Min percent change before stopping

  bool _trim_fp = false;        // True if trim until fix point

  bool _grp_mode = false;       // True if computing group-oriented MUS

  bool _red_mode = false;       // True if using redundancy removal in deletion mode

  bool _reda_mode = false;      // True if using adaptive redundancy removal

  bool _rmr_mode = true;        // True if rotating models using RMR

  bool _reorder_mode = false;   // True is clause reordering is on (with _rmr_mode)

  bool _refine_cset = true;     // Whether to refine working clause set

  bool _mus_mode = true;        // True if computing MUS

  bool _del_mode = true;        // True if computing in deletion mode

  bool _ins_mode = false;       // True if computing in insertion mode

  bool _dich_mode = false;      // True if computing in dichotomic mode

  bool _test_mode = false;      // True if the computed MUS should be tested

  unsigned _order_mode = 0;     // 0 = default (group-id: max->min)
                                // 1 = longest clause first (sum for groups)
                                // 2 = shortest clause first (sum for groups)
                                // 3 = inverse of the default
                                // 4 = random order

};

#endif /* _MUS_CONFIG_H */

/*----------------------------------------------------------------------------*/
