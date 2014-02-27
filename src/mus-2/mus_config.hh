//jpms:bc
/*----------------------------------------------------------------------------*\
 * File:        mus_config.hh
 *
 * Description: Configuration of MUS extractors.
 *
 * Author:      jpms
 * 
 *                                     Copyright (c) 2010, Joao Marques-Silva
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

  void set_prefix(const char* prefix) { _output_prefix = prefix; }

  bool get_comp_format() { return _comp_fmt; }

  void set_comp_format() { _comp_fmt = true; }

  void unset_comp_format() { _comp_fmt = false; }

  void set_comp_mode() { _comp_mode = true; }

  void unset_comp_mode() { _comp_mode = false; }

  bool get_comp_mode() { return _comp_mode; }

  const char* get_output_file() { return _output_file; }

  void set_output_file(const char* ofile) { _output_file = ofile; }

  int get_output_fmt(void) { return _output_fmt; }

  void set_output_fmt(int fmt) { _output_fmt = fmt; }

  const char* get_sat_solver(void) { return _solver; } 

  bool chk_sat_solver(const char* tsolver) { return !strcmp(_solver, tsolver); }

  void set_sat_solver(const char* tsolver) { _solver = tsolver; }

  int get_solpre_mode(void) { return _solpre_mode; }

  void set_solpre_mode(int m) { _solpre_mode = m; }

  bool get_stats() { return _stats; }

  void set_stats() { _stats = true; }

  void unset_stats() { _stats = false; }

  int get_phase() { return _phase; }

  void set_phase(int nph) { _phase = nph; }

  bool get_init_unsat_chk() { return _init_unsat_chk; }

  void set_init_unsat_chk() { _init_unsat_chk = true; }

  void unset_init_unsat_chk() { _init_unsat_chk = false; }

  bool get_incr_mode() { return _incr_mode; }

  void set_incr_mode() { _incr_mode = true; }

  void unset_incr_mode() { _incr_mode = false; }

  void set_sls_mode(bool sm = true) { _sls_mode = sm; }

  bool get_sls_mode(void) override { return _sls_mode; }

  bool get_trim_mode() { return _trim_uset; }

  void set_trim_mode() { _trim_uset = true; }

  void unset_trim_mode() { _trim_uset = false; }

  ULINT get_trim_iter() { return _trim_iter; }

  void set_trim_iter(ULINT niter) {
    _trim_iter = niter;
    if (niter == 0) { unset_trim_mode(); }
    else            { set_trim_mode(); }
  }

  ULINT get_trim_percent() { return _trim_prct; }

  void set_trim_percent(ULINT nprct) {
    _trim_prct = nprct;
    if (nprct == 0) { unset_trim_mode(); }
    else            { set_trim_mode(); }
  }

  bool get_trim_fixpoint() { return _trim_fp; }

  void set_trim_fixpoint() { set_trim_mode(); _trim_fp = true; }

  void unset_trim_fixpoint() { unset_trim_mode(); _trim_fp = false; }

  void set_grp_mode() { _grp_mode = true; }

  void unset_grp_mode() { _grp_mode = false; }

  bool get_grp_mode() { return _grp_mode; }

  void set_trace_enabled() { _trace_on = true; }

  void unset_trace_enabled() { _trace_on = false; }

  bool get_trace_enabled() { return _trace_on; }

  bool get_rm_red_mode() { return _red_mode; }

  void set_rm_red_mode() { _red_mode = true; _reda_mode = false; }

  void unset_rm_red_mode() { _red_mode = false; }

  bool get_rm_reda_mode() { return _reda_mode; }

  void set_rm_reda_mode() { _reda_mode = true; _red_mode = false; }

  void unset_rm_reda_mode() { _reda_mode = false; }

  bool get_refine_clset_mode() { return _refine_cset; }

  void set_refine_clset_mode() { _refine_cset = true; }

  void unset_refine_clset_mode() { _refine_cset = false; }

  bool get_model_rotate_mode() {
    return _rmr_mode || _emr_mode || _imr_mode || _intelmr_mode || _smr_mode;
  }

  void unset_model_rotate_mode() {
    _rmr_mode = _emr_mode = _imr_mode = _intelmr_mode = false;
    _smr_mode = 0;
  }

  bool get_rmr_mode() { return _rmr_mode; }

  void set_rmr_mode() {
    _rmr_mode = true; _emr_mode = false; _imr_mode = false;
    _intelmr_mode = false; _smr_mode = false;
  }

  bool get_emr_mode() { return _emr_mode; }

  void set_emr_mode() {
    _emr_mode = true; _rmr_mode = false;  _imr_mode = false;
    _intelmr_mode = false; _smr_mode = 0;
  }

  unsigned get_rotation_depth() { return _rot_depth; }

  void set_rotation_depth(unsigned rot_depth) { _rot_depth = rot_depth; }

  unsigned get_rotation_width() { return _rot_width; }

  void set_rotation_width(unsigned rot_width) { _rot_width = rot_width; }

  bool get_imr_mode() { return _imr_mode; }

  void set_imr_mode() {
    _imr_mode = true; _emr_mode = false; _rmr_mode = false;
    _intelmr_mode = false;  _smr_mode = 0;
  }

  bool get_intelmr_mode() { return _intelmr_mode; }

  void set_intelmr_mode() {
    _intelmr_mode = true; _imr_mode = false; _emr_mode = false;
    _rmr_mode = false; _smr_mode = 0;
  }

  bool get_ig0_mode() { return _ig0_mode; }

  void set_ig0_mode() { _ig0_mode = true; }

  void unset_ig0_mode() { _ig0_mode = false; }

  bool get_iglob_mode(void) { return _iglob_mode; }

  void set_iglob_mode(bool iglob_mode = true) { _iglob_mode = iglob_mode; }

  unsigned get_smr_mode() { return _smr_mode; }

  void set_smr_mode(unsigned smr_mode) {
    _smr_mode = smr_mode; _intelmr_mode = false; _imr_mode = false;
    _emr_mode = false; _rmr_mode = false; }

  bool get_reorder_mode() { return _reorder_mode; }

  void set_reorder_mode() { _reorder_mode = true; }

  void unset_reorder_mode() { _reorder_mode = false; }

  bool get_mus_mode() { return _mus_mode; }

  void set_mus_mode() { _mus_mode = true; _irr_mode = false; }

  void unset_mus_mode() { _mus_mode = false; }

  bool get_irr_mode() { return _irr_mode; }

  void set_irr_mode() { _irr_mode = true; _mus_mode = false; }

  void unset_irr_mode() { _irr_mode = false; }

  bool get_del_mode() { return _del_mode; }

  void set_del_mode() { 
    _del_mode = true; _ins_mode = false; _dich_mode = false; 
    _chunk_mode = false; _subset_mode = -1; _fbar_mode = false; _prog_mode = false; }

  void unset_del_mode() { _del_mode=false; }

  bool get_ins_mode() { return _ins_mode; }

  void set_ins_mode() { 
    _ins_mode = true; _del_mode = false; _dich_mode = false; 
    _chunk_mode = false; _subset_mode = -1; _fbar_mode = false; _prog_mode = false; }

  void unset_ins_mode() { _ins_mode=false; }

  bool get_dich_mode() { return _dich_mode; }

  void set_dich_mode() { 
    _dich_mode = true; _ins_mode = false; _del_mode = false;    
    _chunk_mode = false; _subset_mode = -1; _fbar_mode = false; _prog_mode = false; }

  void unset_dich_mode() { _ins_mode=false; }

  bool get_chunk_mode() { return _chunk_mode; }

  void set_chunk_mode() { 
    _chunk_mode = true; _del_mode = false; _ins_mode = false; 
    _dich_mode = false; _subset_mode = -1; _fbar_mode = false; _prog_mode = false; }

  void unset_chunk_mode() { _chunk_mode = false; }

  unsigned get_chunk_size() { return _chunk_size; }

  void set_chunk_size(unsigned chunk_size) { _chunk_size = chunk_size; }

  int get_subset_mode() { return _subset_mode; }

  void set_subset_mode(int sm) { 
    if (sm < 0) return;
    _subset_mode = sm; _del_mode = false; _ins_mode = false; 
    _dich_mode = false; _chunk_mode = false; _fbar_mode = false; _prog_mode = false; 
    if ((sm > 0) && (sm < 10))
      set_trace_enabled();
  }

  void unset_subset_mode() { _subset_mode = -1; unset_trace_enabled(); }

  unsigned get_subset_size() { return _subset_size; }

  void set_subset_size(unsigned subset_size) { _subset_size = subset_size; }

  bool get_fbar_mode() { return _fbar_mode; }

  void set_fbar_mode() {
    _fbar_mode = true;
    _subset_mode = -1; _del_mode = false; _ins_mode = false; 
    _dich_mode = false; _chunk_mode = false; _prog_mode = false; 
  }

  void unset_prog_mode() { _prog_mode = false; }

  bool get_prog_mode() { return _prog_mode; }

  void set_prog_mode() {
    _prog_mode = true;
    _subset_mode = -1; _del_mode = false; _ins_mode = false; 
    _dich_mode = false; _chunk_mode = false; _fbar_mode = false; 
  }

  void unset_fbar_mode() { _fbar_mode = false; }

#ifdef MULTI_THREADED

  unsigned get_num_threads() { return _num_threads; }

  void set_num_threads(unsigned num_threads) { _num_threads = num_threads; }

#endif

  bool get_bcp_mode() { return _bcp_mode; }

  void set_bcp_mode() { _bcp_mode=true; }

  void unset_bcp_mode() { _bcp_mode=false; }

  bool get_bce_mode() { return _bce_mode; }

  void set_bce_mode(bool bce_mode = true) { _bce_mode = bce_mode; }

  bool get_bce2_mode() { return _bce2_mode; }

  void set_bce2_mode(bool bce2_mode = true) { _bce2_mode = bce2_mode; }

  bool get_bce_2g0(void) { return _bce_2g0; }

  void set_bce_2g0(bool bce_2g0 = true) { _bce_2g0 = bce_2g0; }

  bool get_bce_ig0(void) { return _bce_ig0; }

  void set_bce_ig0(bool bce_ig0 = true) { _bce_ig0 = bce_ig0; }

  bool get_ve_mode() { return _ve_mode; }

  void set_ve_mode() { _ve_mode=true; }

  void unset_ve_mode() { _ve_mode=false; }

  bool get_test_mode() { return _test_mode; }

  void set_test_mode() { _test_mode = true; }

  void unset_test_mode() { _test_mode = false; }

  bool get_var_mode() { return _var_mode; }

  void set_var_mode() { _var_mode = true; }

  void unset_var_mode() { _var_mode = false; }

  unsigned get_order_mode() { return _order_mode; }

  void set_order_mode(unsigned order_mode) { _order_mode = order_mode; }

  void unset_order_mode() { _order_mode = 0; }

  unsigned get_unsat_limit() { return _unsat_limit; }

  void set_unsat_limit(unsigned unsat_limit) { _unsat_limit = unsat_limit; }

  void set_pc_mode(bool pc_mode = true) { _pc_mode = pc_mode; }
  bool get_pc_mode(void) { return _pc_mode; }

  void set_pc_pol(int pc_pol = 0) { _pc_pol = pc_pol; }
  int get_pc_pol(void) { return _pc_pol; }

  unsigned get_approx_mode(void) const { return _approx_mode; }
  void set_approx_mode(unsigned approxMode = 0) { _approx_mode = approxMode; }

  LINT get_approx_conf_lim(void) const { return _approx_conf_lim; }
  void set_approx_conf_lim(LINT approxConfLim = -1) { _approx_conf_lim = approxConfLim; }

  float get_approx_cpu_lim(void) const { return _approx_cpu_lim; }
  void set_approx_cpu_lim(float approxCpuLim = 0.0f) { _approx_cpu_lim = approxCpuLim; }

  float get_approx_fact(void) const { return _approx_fact; }
  void set_approx_fact(float approxFact = 1.0f) { _approx_fact = approxFact; }

  const char* get_nid_file(void) const { return _nid_file; }
  void set_nid_file(const char* nid_file) { _nid_file = nid_file; }

  // extra parameters
  void set_param1(int param) { _param1 = param; }
  int get_param1(void) { return _param1; }
  void set_param2(int param) { _param2 = param; }
  int get_param2(void) { return _param2; }
  void set_param3(int param) { _param3 = param; }
  int get_param3(void) { return _param3; }
  void set_param4(int param) { _param4 = param; }
  int get_param4(void) { return _param4; }
  void set_param5(int param) { _param5 = param; }
  int get_param5(void) { return _param5; }

  void get_cfgstr(string& cfgstr) {

    if (!_incr_mode)  { cfgstr += " -nonincr"; }

    if (_var_mode) cfgstr += " -var"; 

    if (_grp_mode) cfgstr += " -grp"; 

    if (_irr_mode) { 
      cfgstr += " -irr";
    } else if (!_mus_mode) { 
      cfgstr += " -nomus"; 
    }

    if (_ins_mode) { cfgstr += " -ins"; }

    if (_dich_mode) { cfgstr += " -dich"; }

    if (_chunk_mode) { 
      cfgstr += " -chunk "; 
      cfgstr += convert<unsigned>(_chunk_size); 
    }

    if (_subset_mode >= 0) {
      cfgstr += " -subset ";
      cfgstr += convert<unsigned>(_subset_mode); 
      cfgstr += " ";
      cfgstr += convert<unsigned>(_subset_size); 
      cfgstr += " ";
      cfgstr += convert<unsigned>(_unsat_limit); 
    }

    if (_fbar_mode) { cfgstr += " -fbar"; }
      
    if (_prog_mode) { cfgstr += " -prog"; }

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

    cfgstr += " -"; cfgstr += _solver;
    if (_solpre_mode) { cfgstr += " -solpre "; cfgstr += convert<int>(_solpre_mode); }

#ifdef MULTI_THREADED
    cfgstr += " -nthr "; cfgstr += convert<unsigned>(_num_threads); 
#endif

    if (_red_mode)      { cfgstr += " -rr"; }

    if (_reda_mode)      { cfgstr += " -rra"; }

    if (get_model_rotate_mode()) {
      if (_emr_mode) { 
        cfgstr += " -emr";
        if (_rot_depth != 1) { cfgstr += " -rdepth "; cfgstr += convert<unsigned>(_rot_depth); }
        if (_rot_width != 1) { cfgstr += " -rwidth "; cfgstr += convert<unsigned>(_rot_width); }
      } else if (_imr_mode) {
        cfgstr += " -imr";
      } else if (_intelmr_mode) {
        cfgstr += " -intelmr";
      } else if (_smr_mode) {
        cfgstr += " -smr "; cfgstr += convert<int>(_smr_mode);
      }
      if (_reorder_mode) { cfgstr += " -reorder"; }
      if (!_iglob_mode) { cfgstr += " -bglob"; }
    } else {
      cfgstr += " -norot";
    }
    if (_ig0_mode) { cfgstr += " -ig0"; }

    if (_order_mode) { cfgstr += " -order "; cfgstr += convert<int>(_order_mode); }

    if (_bcp_mode) { cfgstr += " -bcp"; }
    if (_bce_mode) { cfgstr += " -bce"; }
    if (_bce2_mode) { cfgstr += " -bce2"; }
    if (_bce_mode || _bce2_mode) {
      if (_bce_2g0) { cfgstr += " -bce:2g0"; }
      if (_bce_ig0) { cfgstr += " -bce:ig0"; }
    }
    if (_ve_mode) { cfgstr += " -ve"; }

    if (_pc_mode) { 
      cfgstr += " -pc"; 
      cfgstr += " -pc:pol "; cfgstr += convert<int>(_pc_pol); 
    }

    if (_approx_mode) {
      cfgstr += " -approx "; cfgstr += convert<int>(_approx_mode);
      cfgstr += " -approx:cl "; cfgstr += convert<int>(_approx_conf_lim);
      cfgstr += " -approx:tl "; cfgstr += convert<float>(_approx_cpu_lim);
      cfgstr += " -approx:fact "; cfgstr += convert<float>(_approx_fact);
    }

    if (_init_unsat_chk) { cfgstr += " -ichk"; }
    if (_test_mode) { cfgstr += " -test"; }

    cfgstr += " -ph ";
    cfgstr += convert<int>(_phase);

    if (_comp_fmt)   { cfgstr += " -comp"; }
    if (_stats)       { cfgstr += " -st"; }

    cfgstr += " -T ";
    cfgstr += convert<int>(_timeout);

    cfgstr += " -v ";
    cfgstr += convert<int>(_verbosity);

    if (_nid_file != nullptr) { cfgstr += " -nidfile "; cfgstr += _nid_file; }

#if XPMODE
    cfgstr += " -param1 "; cfgstr += convert<unsigned>(_param1);
    cfgstr += " -param2 "; cfgstr += convert<unsigned>(_param2);
    cfgstr += " -param3 "; cfgstr += convert<unsigned>(_param3);
    cfgstr += " -param4 "; cfgstr += convert<unsigned>(_param4);
    cfgstr += " -param5 "; cfgstr += convert<unsigned>(_param5);
#endif    

  }

protected:

  string _cmdstr;

  int _verbosity = 0;

  int _timeout = 0;

  const char* _output_prefix = "c ";

  const char* _output_file = NULL;

  int _output_fmt = 0;      // Controls the way the MUS (or approximation) is
                            // written out: 0 - default (input format),
                            // 1 - unknown first, 2 - GCNF with necessary in g0

  const char* _solver = "glucose";

  int _solpre_mode = 0;     // Controls preprocessing in the SAT solver:
                            //  0 - none, 1 - preprocess before the first call
                            //  2 - preprocess before each call

  bool _comp_fmt = false;

  bool _comp_mode = false;

  bool _write_mus = false;

  bool _stats = false;

  int _phase = 3;

  int _incr_mode = true;

  bool _sls_mode = false;   // True if using SLS solver

  bool _init_unsat_chk = false;

  bool _trim_uset = false;  // True if clset is to be iteratively trimmed

  ULINT _trim_iter = 0;     // Number of trim iterations

  ULINT _trim_prct = 0;     // Min percent change before stopping

  bool _trim_fp = false;    // True if trim until fix point

  bool _grp_mode = false;   // True if computing group-oriented MUS

  bool _trace_on = false;   // True for tracing (subset mode)

  bool _red_mode = false;   // True if using redundancy removal in deletion mode

  bool _rmr_mode = true;    // True if rotating models using RMR

  bool _emr_mode = false;   // True if rotating models using EMR 

  unsigned _rot_depth = 1;  // Rotation depth (0 = unlimited) for EMR

  unsigned _rot_width = 1;  // Rotation width (0 = unlimited) for EMR

  bool _imr_mode = false;   // True if model rotation for irredundancy is used

  bool _reorder_mode = false;// True is clause reordering is on (with _rmr_mode)

  bool _intelmr_mode = false;// True for "intel" model rotation (experimental)

  unsigned _smr_mode = 0;    // > 0 if rotating using Siert MR, value = depth

  bool _ig0_mode = false;    // If true g0 is ignored during rotation (not sound, in general !)

  bool _iglob_mode = true;   // If true, globally necessary clauses are ignored during rotation

  bool _refine_cset = true;  // Whether to refine working clause set

  bool _mus_mode = true;     // True if computing MUS

  bool _irr_mode = false;    // True if computing irredundant subformulas of SAT formulas

  bool _del_mode = true;     // True if computing in deletion mode

  bool _ins_mode = false;    // True if computing in insertion mode

  bool _dich_mode = false;   // True if computing in dichotomic mode

  bool _chunk_mode = false;  // True if computing in chunked mode

  unsigned _chunk_size = 0;  // The size of chunk, 0 means "size of input"

  bool _fbar_mode = false;   // True is computing in FBAR mode

  bool _prog_mode = false;   // True if computing using the progression-based algo

#ifdef MULTI_THREADED
  unsigned _num_threads = 0; // Number of threads to run: 0 = h/w concurrency
#endif

  bool _bcp_mode = false;    // True if BCP-based simplification should be used

  bool _bce_mode = false;    // True if BCE should be applied

  bool _bce2_mode = false;   // True if second BCE should be applied (after VE)

  bool _bce_2g0 = false;     // If True, blocked clauses are not removed, but
                             // instead are moved to group-0,

  bool _bce_ig0 = false;     // If True, group 0 clauses are ignored by BCE
                             // (unsound, in general)

  bool _ve_mode = false;     // True if VE should be applied

  bool _test_mode = false;   // True if the computed MUS should be tested

  bool _var_mode = false;    // True if computing in terms of (groups) of variables rather than clauses

  unsigned _order_mode = 0;  // When non-0 order clauses/groups/variables to schedule
                             // according to some value:
                             //   1 = largest length (sum of lengths, for groups) first
                             //   2 = smallest length first

  bool _reda_mode = false;   // True if using adaptive redundancy removal

  int _subset_mode = -1;     // non-zero if analysing in subset mode [ECAI-12 submission]
                             // -1 -- just use the size, no heuristics

  unsigned _subset_size = 1; // The size of subsets

  unsigned _unsat_limit = 0; // Number of UNSAT outcomes after which to stop subsetting (0 = no limit)

  bool _pc_mode = false;     // true if using output of proof compactor

  int _pc_pol = 0;           // if != 0 set polarity for abbreviations: 1=pos, -1=neg

  unsigned _approx_mode = 0; // approximation mode:
                             //   0 - turned off
                             //   1 - overapproximation: groups with unknown SAT
                             //       outcomes are included in MUS
                             //   2 - underapproximation: groups with unknown SAT
                             //       status are excluded from MUS
                             //   3 - rescheduling: groups with unknown SAT
                             //       status are re-scheduled

  LINT _approx_conf_lim = 1000;// conflict limit per call in approximation mode,
                             // -1 = no conflict

  float _approx_cpu_lim = 0.0f; // CPU time limit per call in approximation mode
                             // 0 = no limit

  float _approx_fact = 2.0f; // the factor to multiply limits in approx_mode 3

  const char* _nid_file = nullptr; // the file with necessary IDs

  int _param1 = 0;           // extra parameter 1 (for experiments)

  int _param2 = 0;           // extra parameter 2 (for experiments)

  int _param3 = 0;           // extra parameter 3 (for experiments)

  int _param4 = 0;           // extra parameter 4 (for experiments)

  int _param5 = 0;           // extra parameter 5 (for experiments)

};

#endif /* _MUS_CONFIG_H */

/*----------------------------------------------------------------------------*/
