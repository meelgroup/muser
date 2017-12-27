/*----------------------------------------------------------------------------* \
 * File:        muser2.cc
 *
 * Description: MUS ExtractoR, version 2
 *
 * Author:      antonb, jpms
 * 
 *                      Copyright (c) 2010-2014, Anton Belov, Joao Marques-Silva
 \*----------------------------------------------------------------------------*/

// Notes:
//
#include <cstdio>
#include <iostream>
#include <queue>
#include <signal.h>
#include <stack>
#include <unistd.h>

#include "basic_group_set.hh"
#include "bce_simplifier.hh"
#include "bcp_simplifier.hh"
#include "ve_simplifier.hh"
#include "cnffmt.hh"
#include "gcnffmt.hh"
#include "pc_cnffmt.hh"
#include "globals.hh"
#include "id_manager.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#ifdef MULTI_THREADED
#include "mus_data_mt.hh"
#endif
#include "mus_extractor.hh"
#include "simplify_bce.hh"
#include "simplify_bcp.hh"
#include "simplify_ve.hh"
#include "test_mus.hh"
#include "tester.hh"
#include "toolcfg.hh"
#include "utils.hh"
#include "vgcnffmt.hh"
#if XPMODE
#include "../minisat-abbr/core/Solver.h"
#endif

using namespace std;

namespace {

  /** Registers the signal handler */
  void register_sig_handlers(void);
  /** The signal handler */
  static void SIG_handler(int signum);
  /** Prints out the header */
  void print_header(ToolConfig& config, const char* fname);
  /** Command line */
  char* parse_cmdline_options(ToolConfig& cfg, int argc, char** argv);
  /** Loads input into group set */
  void load_file(const char* fname, ToolConfig& config, IDManager& imgr,
                 BasicGroupSet& clset);
  /** Reports the results of the computation */
  void report_results(bool interrupted = false);
  /** Tests the computed results: kicks off various testers for that. */
  void test_results(void);
  /** Writes out the MU/GMU/VMU instance (or approximation) to the output file */
  void write_out_results(bool interrupted = false);

  // global data -- accessed from both main and the signal handlers
  ToolConfig config;    // configuration data
  IDManager imgr;       // ID manager
  MUSData* pmd = 0;     // MUSData
}

/*
 * Main entry point
 */     
int main(int argc, char** argv) 
{
  register_sig_handlers();
  Utils::init_random(0);        // change to -1 to get time-based

  //m2.init_all();

  char* filename = parse_cmdline_options(config, argc, argv);

  alarm(config.get_timeout());

  if (filename == NULL) {
    if (config.get_comp_format())
      cout << "c ";
    report("Options but no file name provided? Terminating...");
    exit(3);
  }
  if (config.get_verbosity() >= 0) {
    print_header(config, filename);
  }

  BasicGroupSet gset(config);
  if (config.get_verbosity() > 0)
    report("Parsing ...");
  load_file(filename, config, imgr, gset);
  prt_cfg_cputime("Parsing completed at ");
  cout_pref << "Input size: " << (gset.init_gsize() - config.get_pc_mode())
            << " groups, " << gset.init_size() << " clauses,"
            << " max.var=" << gset.max_var() << flush;
#ifdef XPMODE
  if (config.get_pc_mode())
    cout << ", first_abbr=" << gset.get_first_abbr()
         << ", first_sel=" << gset.get_first_sel();
#endif
  if (config.get_var_mode())
    cout << ", " << gset.vgsize() << " variable groups, " 
         << gset.vsize() << " variables";
  cout << "." << endl;
#ifndef MULTI_THREADED
  MUSData md(gset, config.get_var_mode());
#else
  MUSDataMT md(gset);
#endif
  pmd = &md; // set up the global pointer to be used by utilities

#ifdef XPMODE
  if (config.get_nid_file() != nullptr) {
    ifstream fin(config.get_nid_file());
    int gid = 0, cnt = 0;
    for ( ; (fin >> gid) && gid; ++cnt) { md.mark_necessary(gid); }
    cout_pref << "Read " << cnt << " necessary groups from file." << endl;
  }
#endif
  // some of the workers and work items need to be available later on
  SATChecker schecker(imgr, config); // will be used if we get pass the pre-processing stage
  schecker.set_pre_mode(config.get_solpre_mode());
#ifdef XPMODE
  // +TEMP: implement set_max_problem_var() in the wrapper and remove
  if (config.chk_sat_solver("minisat-abbr")) {
    void* raw_ptr = schecker.solver().get_raw_solver_ptr();
    MinisatAbbr::Solver* s_ptr = static_cast<MinisatAbbr::Solver*>(raw_ptr);
    s_ptr->setMaxIndexVarInit(gset.max_var());
    s_ptr->garbage_final = true;
  }
  // -TEMP
  if (config.chk_sat_solver("glucose") || config.chk_sat_solver("minisat-abbr")) {
    schecker.solver().set_max_problem_var(
        config.get_pc_mode() ? gset.get_first_abbr() - 1 : gset.max_var());
  }
  SimplifyBCP sb(md, config.get_grp_mode()); // may be needed to re-construct solution
  SimplifyVE sv(md, config.get_grp_mode()); // may be needed to re-construct solution
#endif
  report("Running MUSer2 ...");

#ifdef XPMODE
  if (config.get_bcp_mode()) {
    report ("Simplifying using BCP ...");
    BCPSimplifier bs;
    if (!bs.process(sb) || !sb.completed())
      tool_abort("simplfication failed");
    if (sb.conflict()) { // top-level conflict -- handle and get out
      if (config.get_verbosity() > 0) {
        cout_pref << "Top-level conflict during BCP, BCP removed "
                  << (config.get_grp_mode() ? "all groups, " 
                      : "all non-conflict clauses") 
                  <<  " used CPU time: " << sb.cpu_time() << endl;
      }
      goto _reconstruct_and_print;
    }
    if (config.get_verbosity() > 0) {
      cout_pref << "BCP removed " << sb.rcl_count() << " clauses; "
                <<  sb.rg_count() << " groups; "
                <<  " used CPU time: " << sb.cpu_time() << endl;
    }
    prt_cfg_cputime("BCP simplification completed at ");      
  }

  if (config.get_bce_mode()) {
    report ("Doing BCE ...");
    BCESimplifier bs;
    SimplifyBCE sb(md);
    sb.set_destructive(true);
    sb.set_blocked_2g0(config.get_bce_2g0());
    sb.set_ignore_g0(config.get_bce_ig0());
    if (!bs.process(sb) || !sb.completed())
      tool_abort("BCE failed");
    if (config.get_verbosity() > 0) {
      cout_pref << "BCE removed " <<  sb.rcl_count() << " clauses; "
                << sb.rg_count() << " groups; "
                <<  " used CPU time: " << sb.cpu_time() << endl;
    }
    prt_cfg_cputime("BCE completed at ");      
  }

  if (config.get_ve_mode()) {
    report ("Preprocessing using VE ...");
    VESimplifier vs;
    if (!vs.process(sv) || !sv.completed())
      tool_abort("preprocessing failed");
    if (sv.conflict()) { // top-level conflict -- handle and get out
      // TODO !!!: bs.reconstruct_solution(sb);
      if (config.get_verbosity() > 0) {
        cout_pref << "Top-level conflict during VE "
          //, BCP removed " << (config.get_grp_mode() ? "all groups, "  : "all non-conflict clauses") 
                  <<  " used CPU time: " << sv.cpu_time() << endl;
      }
      goto _reconstruct_and_print;
    }
    if (config.get_verbosity() > 0) {
      cout_pref << "VE removed " << sv.rcl_count() << " clauses; "
                <<  sv.rg_count() << " groups; "
                <<  " used CPU time: " << sv.cpu_time() << endl;
    }
  }

  // second BCE
  if (config.get_bce2_mode()) {
    report ("Doing BCE2 ...");
    BCESimplifier bs;
    SimplifyBCE sb(md);
    sb.set_destructive(true);
    sb.set_blocked_2g0(config.get_bce_2g0());
    sb.set_ignore_g0(config.get_bce_ig0());
    if (!bs.process(sb) || !sb.completed())
      tool_abort("BCE2 failed");
    if (config.get_verbosity() > 0) {
      cout_pref << "BCE2 removed " <<  sb.rcl_count() << " clauses; "
                << sb.rg_count() << " groups; "
                <<  " used CPU time: " << sb.cpu_time() << endl;
    }
    prt_cfg_cputime("BCE2 completed at ");      
  }
#endif // XPMODE

  // memory optimization -- get rid of occs list, if its not needed anymore
  if (!config.get_model_rotate_mode() && !config.get_var_mode())
    gset.drop_occs_list();

  // do the trimming or unsat check (note that SATChecker is re-used during)
  // subsequent extraction
  if (config.get_trim_mode()) {
    if (config.get_verbosity() > 0)
      report("Trimming ..."); 
    TrimGroupSet tg(md);
    tg.set_trim_fixpoint(config.get_trim_fixpoint());
    tg.set_iter_limit(config.get_trim_iter());
    tg.set_pct_limit(config.get_trim_percent());
    if (!schecker.process(tg) || !tg.completed())
      tool_abort("trimming failed");
    if (!tg.is_unsat())
      tool_abort("the instance is SATISFIABLE.");
    if (config.get_verbosity() > 0)
      cout_pref << "Group set size after trimming: " << md.real_gsize() 
                << " groups." << endl;
    prt_cfg_cputime("Trimming completed at ");
  } else if (config.get_init_unsat_chk()) {
    if (config.get_verbosity() > 0)
      report("Doing initial (UN)SAT check ...");
    CheckUnsat cu(md);
    if (!schecker.process(cu) || !cu.completed())
      tool_abort("initial (UN)SAT check failed");
    if (config.get_irr_mode()) {
      if (cu.is_unsat())
        tool_abort("the instance is UNSATISFIABLE.");
    } else {
      if (!cu.is_unsat())
        tool_abort("the instance is SATISFIABLE.");
    }
    prt_cfg_cputime("Initial (UN)SAT check completed at ");
  } else {
    report("No trimming and no initial (UN)SAT check ...");
  }
  
  // do the MUS or irredundant formula extraction (if asked for)
  if (config.get_mus_mode() || config.get_irr_mode()) {
    // off we go ...
    MUSExtractor mex(imgr, config);
    mex.set_sat_checker(&schecker);     // re-use the checker
    ComputeMUS cm(md);
    if (!mex.process(cm) || !cm.completed())
      tool_abort("extraction failed, see previous error messages.");
  
    cout_pref << "CPU time of extraction only: " 
              << mex.cpu_time() << " sec" << endl;
#ifdef MULTI_THREADED
    cout_pref << "Wall-clock time of extraction only: " 
              << mex.wc_time() << " sec" << endl;
#endif
#ifndef MULTI_THREADED // TODO: remove when stats are ok for MT
    cout_pref << "Calls to SAT solver during extraction: "      
              << mex.sat_calls() << endl;
    if (config.get_model_rotate_mode()) {
      cout_pref << "Groups detected by model rotation: "
                << mex.rot_groups() << " out of " << md.nec_gids().size() << endl;
    }
    if (config.get_refine_clset_mode()) {
      cout_pref << "Groups removed with refinement: "
                << mex.ref_groups() << " out of " << md.r_gids().size() << endl;
    }
#endif
  }

#ifdef XPMODE
 _reconstruct_and_print:
  // if pre-processing was used, reconstruct the solution
  if (config.get_ve_mode()) {
    VESimplifier vs;
    if (config.get_verbosity() > 0)
      report("Reconstructing solution after VE ...");
    vs.reconstruct_solution(sv);
    if (config.get_verbosity() > 0)
      cout_pref << "Reconstruction used CPU time: " << sv.cpu_time() << endl;
    if (vs.unsound() || vs.unsound_mr())
      cout_pref << "Warning: reconstruction after VE may have been unsound" 
                << "(s=" << vs.unsound() << ", mr=" << vs.unsound_mr() << ")" 
                << endl;
  }

  // if pre-processing was used, reconstruct the solution
  if (config.get_bcp_mode()) {
    BCPSimplifier bs;
    if (config.get_verbosity() > 0)
      report("Reconstructing solution after BCP ...");
    bs.reconstruct_solution(sb);
  }
#endif

  // report results
  report_results();
  // test (if asked for)
  if (config.get_test_mode())
    test_results();
  if (config.get_comp_format()) {
    cout << (config.get_mus_mode() ? "s UNSATISFIABLE" : "s SATISFIABLE") << endl;
    md.write_comp(cout);
  }
  // output the result, if asked (this covers trimming-only path as well) 
  if (config.get_output_file() != NULL)
    write_out_results(!config.get_mus_mode() && !config.get_irr_mode());

  report("Terminating MUSer2 ...");
  prt_cfg_cputime("");
  exit(20);  // return is better for cleanup, but exit is faster (no cleanup)
}

#define TOOL_HELP_HEADER \
"\n" \
"MUSer2: (V/G)MUS/MES extractor and more\n" \
"\n" \
"commit-id: " COMMITID " built: " BUILDDATE "\n""" \
"\n" \
"Usage: muser2 [<option> ... ] <input> \n" \
"where <option> is one of the following:\n"

#define TOOL_HELP_STD_SWITCHES \
" Execution control:\n" \
"  -h        prints this help and exits\n" \
"  -v NNN    verbosity level [default: -v 1]\n" \
"  -T TTT    specify timeout, 0 = no timeout [default: 0]\n" \
"  -comp     use competitions output format [default: off]\n" \
"  -w        write the result instance in default file [default: off]\n" \
"  -wf FFF   write the result instance in file FFF.[g]cnf [default: no writing]\n" \
"  -st       print intermediate stats\n" \
"  -test     test the result for correctness [default: off]\n" \
" Main functionality:\n" \
"  -var      compute variable-MUSes [SAT 2012] [default: off]\n" \
"  -grp      compute group-MUS (input format is gcnf) or VGMUS (input format is vgcnf) [default: off]\n" \
"  -irr      compute MES of SAT formula instead of MUS of UNSAT formula [CP 2012] [default: off, i.e. compute MUS]\n" \
"  -chunk C  use chunked mode for MES computation with chunk of C groups; C=0 means one chunk [CP 2012] [default: off]\n" \
"  -nomus    do not compute MUS, just preprocess and exit [default: off, i.e. computes (group)MUS]\n" \
"  -ins      compute MUS using insertion-based algorithm [TEMP: no groups, vars, MES]\n" \
"  -dich     compute MUS using dichotomic algorithm [TEMP: no groups, vars, MES]\n"     \
" Optimizations and heuristics:\n" \
"  -norf     do not refine target clause sets with unsat subsets [default: off]\n" \
"  -norot    do not detect necessary clauses using model rotation [default: off]\n" \
"  -rr       use redundancy removal [default: off; TEMP: do not use with GCNF]\n" \
"  -rra      use adaptive redundancy removal [default: off; TEMP: do not use with GCNF]\n" \
"  -emr      use extended model rotation (EMR clauses [AIComm-11], variables [SAT 2012]) [default: off]\n" \
"  -imr      use specialized model rotation for MES computation [CP 2012] [default: off]\n" \
"  -bglob    block rotation through globally necessary clauses during rotation [default: off]\n" \
"  -order N  schedule clauses/groups/variables according to some order:\n" \
"              0 = default (group-id: GMUS: max->min; VGMUS: min->max)\n" \
"              1 = longest clause/occlist first (sum for groups)\n" \
"              2 = shortest clause/occlist first (sum for groups)\n" \
"              3 = inverse of the default\n"\
"              4 = random order (TEMP: groups only)\n" \
" Preprocessing:\n" \
"  -trim  N  iterate N times reducing unsat subset [default: off]\n" \
"  -tfp      trim until fix point is reached [default: off]\n" \
"  -tprct P  trim until change is size change is < P% [default: off]\n" \
"  -ichk     do inital unsat check - the difference from -trim 1 is that\n" \
"            there's no refinement [default: off]\n" \
" SAT solver control:\n" \
"  -ph N     global phase in SAT solver 0=false,1=true,2=random,3=solver default [default: 3]\n" \
"  -nonincr  use SAT solver in non-incremental mode [default: off]\n" \
"  -solpre N controls preprocessing in the SAT solver [default: 0]\n" \
"               0 = no preprocessing\n" \
"               1 = preprocess before the first SAT call only\n" \
"               2 = preprocess before each SAT call\n" \
"  -glucose  use the Glucose 3.0 solver (incr. only) [default: on]\n" \
"  -glucoses same as above, but with SatElite (incr. only) [default: off]\n" \
"  -minisat  use Minisat 2.2 SAT solver (incr. only) [default: off, , recommended with -grp]\n" \
"  -minisats use Minisat 2.2 SAT solver and do SatELite pre-processing (incr. only) [default: off, recommended with -grp]\n" \
"  -minisat-gh use the version of Minisat 2.2.0 from github (incr. only) [default: off]\n" \
"  -minisat-ghs same as above, but with SatElite (incr. only) [default: off]\n" \
"  -picosat  use the picosat-954 SAT solver [default: off] \n" \
"\n"

#define TOOL_HELP_EXP_SWITCHES \
" EXPERIMENTAL OPTIONS:\n" \
" Execution control:\n" \
"  -wfmt NNN output format for -w or -wf when in plain CNF mode [default: 0]\n" \
"            0 - output plain CNF; 1 - plain CNF, but put unknown clauses first;\n" \
"            2 - output group-CNF with necessary clauses in group-0\n" \
" Main functionality:\n" \
"  -subset M S L   use subset mode M with subsets of size S>0 and UNSAT outcomes\n" \
"            limit L>=0, 0 means no limit [default: off]\n" \
"            M=0 - current default ordering\n" \
"            M=1 - path count\n" \
"            M=2(3) - use true(false) support of articulation points\n" \
"            M=4(5) - same as M=2(3) but when L is reached, use path counts to detect necessary clauses\n" \
"            M=10 - take up to S clauses from current ordering\n" \
"            M=11 - take up to (S-1) clauses from 1-hood of the clause in current ordering\n" \
"                   S=0 means take all 1-hood\n" \
"  -fbar     enable specialized algorithm for flop-based abstraction refinement [default: off]\n" \
"            TEMP: forces trim mode\n"\
"  -prog     enable progression-based MUS computation [default: off]\n" \
"  -nidfile FILE  path to the file that contains a list of necessary group-IDs\n" \
"            the file is expected one line with space-separated group-IDs, terminated\n" \
"            by 0 [default: none]\n" \
" Optimizations and heuristics:\n" \
"  -order N  schedule clauses/groups/variables according to some order:\n" \
"              5 = largest rgraph degree first (TEMP: clauses only)\n" \
"              6 = smallest rgraph degree first (TEMP: clauses only)\n" \
"              9 = largest implicit rgraph degree first\n" \
"              10 = smallest implicit rgraph degree first\n" \
"              11 = largest implicit cgraph degree first\n" \
"              12 = smallest implicit cgraph degree first\n" \
"  -reorder  use clause reordering when using model rotation [default: off]\n" \
"  -rdepth D specify model rotation depth for EMR, 0 for clauses means unlimited [default: 1]\n" \
"  -rwidth W specify model rotation width for EMR, 0 means unlimited [default: 1]\n" \
"  -intelmr  use model rotation for Intel-like instances (experimental) [default: off]\n" \
"  -smr D    use SMR (Wieringa CP12) with depth D (>0) [default: off]\n" \
"  -ig0      ignore clauses falsified in g0 during rotation [default: off; NOTE: not sound, in general]\n" \
" Preprocessing:\n" \
"  -bcp      simplify instance using BCP [default: off]\n" \
"  -bce      simplify instance using BCE before VE [default: off]\n" \
"  -bce2     simplify instance using BCE after VE [default: off]\n" \
"  -bce:2g0  move blocked clauses into g0 during BCE, instead of removing them [default: off]\n" \
"  -bce:ig0  ignore g0 clauses during BCE (unsound, in general) [default: off]\n" \
"  -ve       simplify instance using VE [default: off; TEMP: do not use with -bcp]\n" \
" SAT solver control:\n" \
"  -minisat-hmuc use the proof-tracing version of minisat from Haifa-MUC (nonincr. only) [Ryvchin, Strichman, SAT-2012] [default: off]\n" \
"  -minisat-abbr use the abbreviating version of minisat (incr. only) [Lagniez, Biere, SAT-2013] [default: off]\n" \
"  -lingeling use the lingeling-ala solver (incr. only) [default: off] \n" \
"  -picosat935 use the picosat-935 SAT solver [default: off] \n" \
"  -ipasir use the attached IPASIR solver library (if compiled in) [default: off] \n" \
"  -ubcsat12 use UBCSAT 1.2 solver; if set, make sure to set approximation mode [default: off] \n" \
" Proof-compactor:\n" \
"  -pc       assume that the input formula is an output of Marijn's proof compactor [default: off]\n" \
"  -pc:pol N if not 0, set polarity of abbreviations to true(1) or false(-1) [default: 0]\n" \
" Approximation (hybrid algorithm only):\n" \
"  -approx N controls the approximation mode: in approximation modes SAT calls\n" \
"            are resource restricted (see -approx:cl and -approx:tl), and might\n" \
"            return the 'unknown' status\n" \
"            0 - turned off [default]\n" \
"            1 - overapproximation: groups with unknown SAT outcomes are included\n" \
"                in MUS\n" \
"            2 - underapproximation: groups with unknown SAT outcomes are excluded\n" \
"                from MUS\n" \
"            3 - rescheduling [TEMP: not implemented yet]: groups with unknown SAT outcomes are rescheduled,\n" \
"                and the limits for these groups are multiplied by -approx:fact\n" \
"  -approx:cl N the conflict limit per SAT call, -1 = no limit [default: 1000]\n" \
"  -approx:tl F.F the CPU limit per SAT call, 0 = no limit [default: 0]\n" \
"  -approx:fact F.F the factor to multiply the limits by [default: 2.0]\n" \
""

namespace {

  void prt_help() {
    cout << TOOL_HELP_HEADER;
    cout << TOOL_HELP_STD_SWITCHES;
#ifdef XPMODE
    cout << TOOL_HELP_EXP_SWITCHES;
#endif
    cout << "Authors:      " << authorname << " (" << authoremail << ")" << endl;
    if (strcmp(contribs, "")) {
      cout << "Contributors: " << contribs << endl;
    }
  }

  char* parse_cmdline_options(ToolConfig& cfg, int argc, char** argv) 
  {
    DBG(cout << "ARGC: " << argc << endl; cout.flush(););

    if (argc == 1) {
      prt_help();
      exit(1);
    }
    for (int i = 1; i < argc - 1; ++i) {
      cfg.append_cmdstr((const char*) argv[i]);
    }
    for (int i = 1; i < argc;) {
      NDBG(cout << "Current argv: " << argv[i] << endl;);
      if (!strcmp(argv[i], "-h")) {
        prt_help();
        exit(1);
      }
      else if (!strcmp(argv[i], "-grp")) { cfg.set_grp_mode(); } 
      else if (!strcmp(argv[i], "-T")) { cfg.set_timeout(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-v")) { 
        ++i;
        cfg.set_verbosity(atoi(argv[i]));
      } else if (!strcmp(argv[i], "-comp")) {
        cfg.set_comp_format();
      } else if (!strcmp(argv[i], "-st")) {
        cfg.set_stats();
      } else if (!strcmp(argv[i], "-w")) {
        cfg.set_output_file(output_file);
      } else if (!strcmp(argv[i], "-wf")) {
        ++i;
        cfg.set_output_file(argv[i]);
      }
#ifdef MULTI_THREADED
      else if (!strcmp(argv[i], "-nthr")) {++i; cfg.set_num_threads(atoi(argv[i]));}
#endif
      else if (!strcmp(argv[i], "-ph")) {++i; cfg.set_phase(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-nonincr")) {cfg.unset_incr_mode();}
      else if (!strcmp(argv[i], "-solpre")) { cfg.set_solpre_mode(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-picosat")) {cfg.set_sat_solver("picosat");}
      else if (!strcmp(argv[i], "-minisat")) {cfg.set_sat_solver("minisat");}
      else if (!strcmp(argv[i], "-minisats")) {cfg.set_sat_solver("minisats");}
      else if (!strcmp(argv[i], "-minisat-gh")) {cfg.set_sat_solver("minisat-gh");}
      else if (!strcmp(argv[i], "-minisat-ghs")) {cfg.set_sat_solver("minisat-ghs");}
      else if (!strcmp(argv[i], "-glucose")) {cfg.set_sat_solver("glucose");}
      else if (!strcmp(argv[i], "-glucoses")) {cfg.set_sat_solver("glucoses");}
      //
      else if (!strcmp(argv[i], "-trim")) {++i; cfg.set_trim_iter(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-tprct")) {++i; cfg.set_trim_percent(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-tfp")) {cfg.set_trim_fixpoint();}
      //
      else if (!strcmp(argv[i], "-nomus")) {cfg.unset_mus_mode();}
      else if (!strcmp(argv[i], "-norf")) {cfg.unset_refine_clset_mode();}
      else if (!strcmp(argv[i], "-norot")) {cfg.unset_model_rotate_mode();}
      else if (!strcmp(argv[i], "-ichk")) {cfg.set_init_unsat_chk();}
      else if (!strcmp(argv[i], "-test")) {cfg.set_test_mode();}
      else if (!strcmp(argv[i], "-emr")) {cfg.set_emr_mode();}
      else if (!strcmp(argv[i], "-var")) {cfg.set_var_mode();}
      else if (!strcmp(argv[i], "-order")) { ++i; cfg.set_order_mode(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-rr")) {cfg.set_rm_red_mode();}
      else if (!strcmp(argv[i], "-rra")) {cfg.set_rm_reda_mode();}
      else if (!strcmp(argv[i], "-irr")) {cfg.set_irr_mode();}
      else if (!strcmp(argv[i], "-imr")) {cfg.set_imr_mode();}
      else if (!strcmp(argv[i], "-bglob")) { cfg.set_iglob_mode(false); }
      else if (!strcmp(argv[i], "-chunk")) { cfg.set_chunk_mode(); ++i; cfg.set_chunk_size(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-ins")) {cfg.set_ins_mode();}  
      else if (!strcmp(argv[i], "-dich")) {cfg.set_dich_mode();}  
#ifdef XPMODE
      else if (!strcmp(argv[i], "-wfmt")) { ++i; cfg.set_output_fmt(atoi(argv[i])); }
      else if (!strcmp(argv[i], "-nidfile")) { cfg.set_nid_file(argv[++i]); }
      else if (!strcmp(argv[i], "-rdepth")) {++i; cfg.set_rotation_depth(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-reorder")) {cfg.set_reorder_mode();}
      else if (!strcmp(argv[i], "-minisat-hmuc")) {cfg.set_sat_solver("minisat-hmuc");}
      else if (!strcmp(argv[i], "-minisat-abbr")) {cfg.set_sat_solver("minisat-abbr");}
      else if (!strcmp(argv[i], "-lingeling")) {cfg.set_sat_solver("lingeling");}
      else if (!strcmp(argv[i], "-picosat935")) {cfg.set_sat_solver("picosat935");}
#ifdef IPASIR_LIB
      else if (!strcmp(argv[i], "-ipasir")) {cfg.set_sat_solver("ipasir");}
#endif
      else if (!strcmp(argv[i], "-ubcsat12")) {cfg.set_sat_solver("ubcsat12"); cfg.set_sls_mode(); cfg.unset_incr_mode(); }
      else if (!strcmp(argv[i], "-rwidth")) {++i; cfg.set_rotation_width(atoi(argv[i]));}
      else if (!strcmp(argv[i], "-intelmr")) { cfg.set_intelmr_mode(); }
      else if (!strcmp(argv[i], "-smr")) { cfg.set_smr_mode(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-bcp")) {cfg.set_bcp_mode();}
      else if (!strcmp(argv[i], "-bce")) {cfg.set_bce_mode();}
      else if (!strcmp(argv[i], "-bce2")) {cfg.set_bce2_mode();}
      else if (!strcmp(argv[i], "-bce:2g0")) { cfg.set_bce_2g0(); }
      else if (!strcmp(argv[i], "-bce:ig0")) { cfg.set_bce_ig0(); }
      else if (!strcmp(argv[i], "-ve")) {cfg.set_ve_mode();}
      else if (!strcmp(argv[i], "-ig0")) {cfg.set_ig0_mode();}
      else if (!strcmp(argv[i], "-subset")) {
        cfg.set_subset_mode(atoi(argv[++i]));
        cfg.set_subset_size(atoi(argv[++i]));
        cfg.set_unsat_limit(atoi(argv[++i]));
      }
      else if (!strcmp(argv[i], "-fbar")) {cfg.set_fbar_mode();}
      else if (!strcmp(argv[i], "-prog")) {cfg.set_prog_mode();}
      else if (!strcmp(argv[i], "-pc")) {cfg.set_pc_mode();}
      else if (!strcmp(argv[i], "-pc:pol")) { cfg.set_pc_pol(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-approx")) { cfg.set_approx_mode(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-approx:cl")) { cfg.set_approx_conf_lim(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-approx:tl")) { cfg.set_approx_cpu_lim(atof(argv[++i])); }
      else if (!strcmp(argv[i], "-approx:fact")) { cfg.set_approx_fact(atoi(argv[++i])); }
      else if (!strcmp(argv[i], "-param1")) { cfg.set_param1(atoi(argv[++i]));}
      else if (!strcmp(argv[i], "-param2")) { cfg.set_param2(atoi(argv[++i]));}
      else if (!strcmp(argv[i], "-param3")) { cfg.set_param3(atoi(argv[++i]));}
      else if (!strcmp(argv[i], "-param4")) { cfg.set_param4(atoi(argv[++i]));}
      else if (!strcmp(argv[i], "-param5")) { cfg.set_param5(atoi(argv[++i]));}
#endif
      //
      else { // Specify filename
        DBG(cout << "File: " << argv[i] << endl;);
        return argv[i];
      }
      ++i;
    }
    return NULL;
  }

  //jpms:bc
  /*----------------------------------------------------------------------------*\
   * Purpose: Load CNF or GCNF file.
   \*----------------------------------------------------------------------------*/
  //jpms:ec

  void load_file(const char* fname, ToolConfig& config, IDManager& imgr,
                 BasicGroupSet& gset) {
    gzFile in = gzopen(fname, "rb");
    if (in == Z_NULL) {
      string msg("Unable to open file: ");
      msg += fname;
      tool_abort(msg.c_str());
    }
    assert(in != Z_NULL);
    if (!config.get_grp_mode()) {
      if (config.get_pc_mode()) {
        PCCNFParserTmpl<BasicGroupSet> parser;
        parser.load_cnf_file(in, imgr, gset);
      } else {
        CNFParserTmpl<BasicGroupSet> parser;
        parser.load_cnf_file(in, imgr, gset);
      }
    } else {
      if (!config.get_var_mode()) {
        GroupCNFParserTmpl<BasicGroupSet> parser;
        parser.load_gcnf_file(in, imgr, gset);
      } else {
        VarGroupCNFParserTmpl<BasicGroupSet> parser;
        parser.load_vgcnf_file(in, imgr, gset);
      }
    }
    gzclose(in);
    gset.set_init_size(gset.size());
    gset.set_init_gsize(gset.gsize());
  }

  //jpms:bc
  /*----------------------------------------------------------------------------*\
   * Purpose: Print runtime header when executing Muser.
   \*----------------------------------------------------------------------------*/
  //jpms:ec

  void print_header(ToolConfig& config, const char* fname) {
    cout_pref << "*** " << toolname << ": a MUS extractor ***" << endl;
    cout_pref << "*** commit-id: " << commit_id << " ***" << endl;
    cout_pref << "*** built: " << build_date << " ***" << endl;
#ifdef MULTI_THREADED
    cout_pref << "*** build type: << multi-threaded ***" << endl;
#endif
    cout_pref << "*** authors: " << authorname << " (" << authoremail << ") ***"
              << endl;
    if (strcmp(contribs, "")) {
      cout_pref << "*** contributors: " << contribs << " ***" << endl;
    }
    cout_pref << endl;
    cout_pref << "*** instance: " << fname << " ***" << endl;
    string cfgstr; config.get_cfgstr(cfgstr);
    cout_pref<<"*** config:"<<cfgstr<<" ***"<<endl;
    cout_pref << endl;
  }

  /* Registers the signal handler */
  void register_sig_handlers(void) {
    signal(SIGHUP,SIG_handler); signal(SIGINT,SIG_handler); 
    signal(SIGQUIT,SIG_handler); signal(SIGTERM,SIG_handler); 
    signal(SIGABRT,SIG_handler); signal(SIGALRM,SIG_handler);
  }

  /* Signal handler */
  void SIG_handler(int signum)
  {
    cout << endl;
    if (signum == SIGALRM) {
      cout << "c Timeout (" << config.get_timeout() << " sec)" << endl;
    } else {
      string signame;
      switch (signum) {
      case SIGHUP: signame = "SIGHUP"; break;
      case SIGINT: signame = "SIGINT"; break; 
      case SIGQUIT: signame = "SIGQUIT"; break;
      case SIGTERM: signame = "SIGTERM"; break;
      default: tool_abort("unxpected signal"); break;
      }
      cout << "c Received signal " << signame << ", terminating." << endl;
    }
    // report (partial) results
    report_results(true);
    if (config.get_comp_format()) {
      cout << "s UNKNOWN" << endl;
      if (pmd)
        pmd->write_comp(cout);
    }
    // output the result, if asked (this covers trimming-only path as well) 
    if (config.get_output_file() != NULL)
      write_out_results(true);
    // done
    report("Terminating MUSer2 ...");
    prt_cfg_cputime("");
    exit(0);  // return is better for cleanup, but exit is faster (no cleanup)
  }

  /* Reports the results of the computation; accesses the global data; if
   * interrupted = true assumes that the computation has not been completed
   * and so the results are approximate
   */
  void report_results(bool interrupted)
  {
    if (interrupted)
      cout_pref << "WARNING: the tool was interrupted; results are approximate." 
                << endl;
    if (pmd == 0)
      tool_abort("Got interrupted before any results were obtained.");
    MUSData& md = *pmd;
    BasicGroupSet& gset = md.gset();

    if (!config.get_var_mode()) { // clause mode
      if (config.get_mus_mode() || config.get_irr_mode()) {
        unsigned init_size = gset.init_gsize() - (config.get_grp_mode() && gset.has_g0()); // g0 is counted
        unsigned curr_size = init_size - md.r_gids().size();
        if (config.get_pc_mode()) { --init_size; --curr_size; }
        assert(interrupted || (curr_size == md.nec_gids().size()));
        cout_pref << (config.get_mus_mode() ? "MUS" : "Irredundant subformula") 
                  << ((interrupted || md.pot_nec_gids().size()) ? "(over-approximation)" : "") << " size: "
                  << curr_size << " out of " << init_size
                  << (config.get_grp_mode() ? " groups" : " clauses")
                  << " (" << 100.0*curr_size/init_size << "%)" 
                  << endl;
        if (md.pot_nec_gids().size()) {
          cout_pref << md.pot_nec_gids().size() << " clauses are not proved to be necessary." << endl;
        }
      } 
    } else { // variable mode
      if (config.get_mus_mode()) {
        unsigned init_size = gset.vgsize();
        unsigned curr_size = init_size - md.r_gids().size();
        assert(interrupted || (curr_size == md.nec_gids().size()));
        // compute induced formula size
        unsigned if_size = 0;
        for (cvec_iterator pcl = gset.begin(); pcl != gset.end(); ++pcl)
          if_size += !((*pcl)->removed());
        cout_pref << "VMUS "
                  << (interrupted ? "over-approximation " : "") << "size: "
                  << curr_size << " out of " << init_size
                  << (config.get_grp_mode() ? " variable groups" : " variables")          
                  << " (" << 100.0*curr_size/init_size << "%)" 
                  << ", induced subformula size: " << if_size << " clauses."
                  << endl;
      } else if (config.get_irr_mode()) {
        tool_abort("Redundancy removal for variables is not implemented");
      }
    }
  }

  /* Tests the computed results: kicks off various testers for that.
   */
  void test_results(void)
  {
    if (pmd == 0)
      tool_abort("Got interrupted before any results were obtained.");
    MUSData& md = *pmd;

    if (!config.get_var_mode()) {
      if (config.get_mus_mode()) {
        TestMUS tm(md);
        Tester mt(imgr, config);
        report("Testing the computed MUS ...");
        if (!mt.process(tm) || !tm.completed())
          tool_abort("testing failed");
        cout_pref << "Testing completed, result: " << tm.result_string() << endl;
        cout_pref << "Testing used CPU Time: " << tm.cpu_time() 
                  << ", SAT calls: " << tm.sat_calls()
                  << ", rotated: " << tm.rot_groups()
                  << endl;
      } else if (config.get_irr_mode()) {
        TestIrr ti(md);
        Tester mt(imgr, config);
        report("Testing the computed subformula ...");
        if (!mt.process(ti) || !ti.completed())
          tool_abort("testing failed");
        cout_pref << "Testing completed, result: " << ti.result_string() << endl;
        cout_pref << "Testing used CPU Time: " << ti.cpu_time() 
                  << ", SAT calls: " << ti.sat_calls()
                  << endl;
      }
    } else { // variable mode
      if (config.get_mus_mode()) {
        TestVMUS tm(md);
        Tester mt(imgr, config);
        report("Testing the computed VMUS ...");
        if (!mt.process(tm) || !tm.completed())
          tool_abort("testing failed");
        cout_pref << "Testing completed, result: " << tm.result_string() << endl;
        cout_pref << "Testing used CPU Time: " << tm.cpu_time() 
                  << ", SAT calls: " << tm.sat_calls()
                  << ", rotated: " << tm.rot_groups()
                  << endl;
      } else if (config.get_irr_mode()) {
        tool_abort("Redundancy removal for variables is not implemented");
      }
    }
  }

  /* Writes out the MU/GMU/VMU instance (or approximation) to the output file;
   * if interrupted = true assumes that the computation has not been completed
   * and so the results are approximate
   */
  void write_out_results(bool interrupted)
  {
    if (pmd == 0)
      tool_abort("Got interrupted before any results were obtained.");
    MUSData& md = *pmd;
    if (config.get_output_file() == NULL)
      throw logic_error("write_out_results() is called without output file");

    string oname(config.get_output_file());
    oname += (config.get_grp_mode()) 
      ? (config.get_var_mode() ? ".vgcnf" : ".gcnf") : ((config.get_output_fmt() == 2 ) ? ".gcnf" : ".cnf");
    ofstream outf(oname.c_str());
    if (!outf)
      tool_abort("unable to open output file for writing");
    if (!config.get_var_mode()) {
      if (config.get_grp_mode()) 
        md.write_gcnf(outf);
      else
        md.write_cnf(outf,
            config.get_pc_mode() || (config.get_bce_mode() && config.get_bce_2g0()),
            config.get_output_fmt());
    } else {
      if (!config.get_grp_mode())
        md.write_induced_cnf(outf);
      else
        md.write_induced_vgcnf(outf);
    }
  }

} // anonymous namespace

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Purpose: 
 \*----------------------------------------------------------------------------*/
//jpms:ec

/*----------------------------------------------------------------------------*/
