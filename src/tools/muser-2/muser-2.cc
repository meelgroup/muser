/*----------------------------------------------------------------------------*\
 * File:        muser.cc
 *
 * Description: main entry point
 *
 * Author:      jpms, antonb
 * 
 *                      Copyright (c) 2010-2012, Joao Marques-Silva, Anton Belov
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
#include "cnffmt.hh"
#include "gcnffmt.hh"
#include "globals.hh"
#include "id_manager.hh"
#include "mus_config.hh"
#include "mus_data.hh"
#include "mus_extractor.hh"
#include "test_mus.hh"
#include "tester.hh"
#include "toolcfg.hh"

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


/*----------------------------------------------------------------------------*\
 * Purpose: MUS extractor entry-point.
 \*----------------------------------------------------------------------------*/

int main(int argc, char** argv) 
{
  register_sig_handlers();

  char* filename = parse_cmdline_options(config, argc, argv);

  alarm(config.get_timeout());

  if (filename == NULL) {
    if (config.get_comp_format())
      cout << "c ";
    report("Options but no file name provided? Terminating...");
    exit(3);
  }
  if (config.get_verbosity() >= 0)
    print_header(config, filename);

  BasicGroupSet gset(config);
  if (config.get_verbosity() > 0)
    report("Parsing ...");
  load_file(filename, config, imgr, gset);
  prt_cfg_cputime("Parsing completed at ");
  cout_pref << "Input size: " << gset.init_gsize() << " groups, "
            << gset.init_size() << " clauses." << endl;
  MUSData md(gset);
  pmd = &md; // set up the global pointer to be used by utilities
  
  // some of the workers and work items need to be available later on
  SATChecker schecker(imgr, config); // will be used if we get pass the pre-processing stage

  report("Running MUSer2 ...");

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
      report("Doing initial UNSAT check ...");
    CheckUnsat cu(md);
    if (!schecker.process(cu) || !cu.completed())
      tool_abort("initial UNSAT check failed");
    if (!cu.is_unsat())
      tool_abort("the instance is SATISFIABLE.");
    prt_cfg_cputime("Initial UNSAT check completed at ");
  } else {
    report("No trimming and no initial UNSAT check ...");
  }
  
  // do the MUS extraction (if asked for)
  if (config.get_mus_mode()) {
    // off we go ...
    MUSExtractor mex(imgr, config);
    mex.set_sat_checker(&schecker);     // re-use the checker
    ComputeMUS cm(md);
    if (!mex.process(cm) || !cm.completed())
      tool_abort("extraction failed, see previous error messages.");
    cout_pref << "CPU time of MUS extraction only: " 
              << mex.cpu_time() << " sec" << endl;
    cout_pref << "Calls to SAT solver during MUS extraction: "      
              << mex.sat_calls() << endl;
    if (config.get_model_rotate_mode()) {
      cout_pref << "Groups detected by model rotation: "
                << mex.rot_groups() << " out of " << md.nec_gids().size() << endl;
    }
    if (config.get_refine_clset_mode()) {
      cout_pref << "Groups removed with refinement: "
                << mex.ref_groups() << " out of " << md.r_gids().size() << endl;
    }
  }

  // report results
  report_results();
  // test (if asked for)
  if (config.get_test_mode())
    test_results();
  if (config.get_comp_format()) {
    cout << "s UNSATISFIABLE" << endl;
    md.write_comp(cout);
  }
  // output the result, if asked (this covers trimming-only path as well) 
  if (config.get_output_file() != NULL)
    write_out_results(!config.get_mus_mode());

  report("Terminating MUSer2 ...");
  prt_cfg_cputime("");
  exit(20);  // return is better for cleanup, but exit is faster (no cleanup)
}


#define TOOL_HELP_HEADER \
"\n" \
"MUSer2: (G)MUS extractor and more\n" \
"\n" \
"built: " BUILDDATE "\n""" \
"\n" \
"Usage: muser2 [ <option> ... ] <input> \n" \
"where <option> is one of the following:\n"

#define TOOL_HELP_STD_SWITCHES \
" Execution control:\n" \
"  -h        prints this help and exits\n" \
"  -v NNN    verbosity level [default: -v 1]\n" \
"  -T TTT    specify timeout [default: -T 3600]\n" \
"  -comp     use competitions output format [default: off]\n" \
"  -w        write the result instance in default file [default: off]\n" \
"  -wf FFF   write the result instance in file FFF.[g]cnf [default: no writing]\n" \
"  -test     test the result for correctness [default: off]\n" \
" Main functionality:\n" \
"  -grp      compute group-MUS (input format is gcnf) or VGMUS (input format is vgcnf) [default: off]\n" \
"  -nomus    do not compute MUS, just preprocess and exit [default: off, i.e. computes (group)MUS]\n" \
"  -ins      compute MUS using insertion-based algorithm [TEMP: no groups, vars, MES]\n" \
"  -dich     compute MUS using dichotomic algorithm [TEMP: no groups, vars, MES]\n"     \
" Optimizations and heuristics:\n" \
"  -norf     do not refine target clause sets with unsat subsets [default: off]\n" \
"  -norot    do not detect necessary clauses using model rotation [default: off]\n" \
"  -rr       use redundancy removal [default: off; TEMP: do not use with GCNF]\n" \
"  -rra      use adaptive redundancy removal [default: off; TEMP: do not use with GCNF]\n" \
"  -order N  schedule clauses/groups/variables according to some order:\n" \
"              0 = default (group-id: GMUS: max->min; VGMUS: min->max)\n" \
"              1 = longest clause/occlist first (sum for groups)\n" \
"              2 = shortest clause/occlist first (sum for groups)\n" \
"              3 = inverse of the default\n"\
"              4 = random order (TEMP: groups only)\n" \
"  -reorder  use RMR-based clause reordering (FMCAD 2011) [default: off]\n" \
" Preprocessing:\n" \
"  -trim  N  iterate N times reducing unsat subset [default: off]\n" \
"  -tfp      trim until fix point is reached [default: off]\n" \
"  -tprct P  trim until change is size change is < P% [default: off]\n" \
"  -ichk     do inital unsat check - the difference from -trim 1 is that\n" \
"            there's no refinement [default: off]\n" \
" SAT solver control:\n" \
"  -ph NNN   global phase in SAT solver 0=false,1=true,2=random,3=solver default [default: 3]\n" \
"  -minisat  use Minisat 2.2 SAT solver [default: on]\n" \
"  -minisats use Minisat 2.2 SAT solver and do SatELite pre-processing [default: off, recommended with -grp]\n" \
"  -picosat  use the picosat-935 SAT solver [default: off] \n" \
"  -nonincr  use SAT solver in non-incremental mode [default: off]\n"  \
"\n"

#define TOOL_HELP_TAIL \
"authors:    Anton Belov, Joao Marques-Silva [anton.belov,jpms]@ucd.ie\n" \
"\n"

namespace {

  void prt_help() {
    cout << TOOL_HELP_HEADER;
    cout << TOOL_HELP_STD_SWITCHES;
    cout << TOOL_HELP_TAIL;
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
      // execution control
      if (!strcmp(argv[i], "-h")) { prt_help(); exit(1); }
      else if (!strcmp(argv[i], "-v")) cfg.set_verbosity(atoi(argv[++i]));
      else if (!strcmp(argv[i], "-T")) cfg.set_timeout(atoi(argv[++i]));
      else if (!strcmp(argv[i], "-comp")) cfg.set_comp_format();
      else if (!strcmp(argv[i], "-w")) cfg.set_output_file(output_file);
      else if (!strcmp(argv[i], "-wf")) cfg.set_output_file(argv[++i]);
      else if (!strcmp(argv[i], "-test")) cfg.set_test_mode();
      // main functionality
      else if (!strcmp(argv[i], "-grp")) cfg.set_grp_mode();
      else if (!strcmp(argv[i], "-nomus")) cfg.unset_mus_mode();
      else if (!strcmp(argv[i], "-ins")) cfg.set_ins_mode();  
      else if (!strcmp(argv[i], "-dich")) cfg.set_dich_mode();  
      // optimizations and heuristics
      else if (!strcmp(argv[i], "-norf")) cfg.unset_refine_clset_mode();
      else if (!strcmp(argv[i], "-norot")) cfg.unset_model_rotate_mode();
      else if (!strcmp(argv[i], "-rr")) cfg.set_rm_red_mode();
      else if (!strcmp(argv[i], "-rra")) cfg.set_rm_reda_mode();
      else if (!strcmp(argv[i], "-order")) cfg.set_order_mode(atoi(argv[++i]));
      else if (!strcmp(argv[i], "-reorder")) cfg.set_reorder_mode();
      // preprocessing
      else if (!strcmp(argv[i], "-trim")) cfg.set_trim_iter(atoi(argv[++i]));
      else if (!strcmp(argv[i], "-tprct")) cfg.set_trim_percent(atoi(argv[++i]));
      else if (!strcmp(argv[i], "-tfp")) cfg.set_trim_fixpoint();
      else if (!strcmp(argv[i], "-ichk")) cfg.set_init_unsat_chk();
      // SAT solver control
      else if (!strcmp(argv[i], "-ph")) cfg.set_phase(atoi(argv[++i]));
      else if (!strcmp(argv[i], "-minisat")) cfg.set_sat_solver("minisat");
      else if (!strcmp(argv[i], "-minisats")) cfg.set_sat_solver("minisats");
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
      CNFParserTmpl<BasicGroupSet> parser;
      parser.load_cnf_file(in, imgr, gset);
    } else {
      GroupCNFParserTmpl<BasicGroupSet> parser;
      parser.load_gcnf_file(in, imgr, gset);
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
    cout_pref << "*** built: " << build_date << " ***" << endl;
    cout_pref << "*** author: " << authorname << " (" << authoremail << ") ***"
              << endl;
    if (!strcmp(contribs, "")) {
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
    exit(1);  // return is better for cleanup, but exit is faster (no cleanup)
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

    if (config.get_mus_mode()) {
      unsigned init_size = gset.init_gsize() - config.get_grp_mode(); // g0 is counted
      unsigned curr_size = init_size - md.r_gids().size();
      assert(interrupted || (curr_size == md.nec_gids().size()));
      cout_pref << (config.get_mus_mode() ? "MUS " : "Irredundant subformula ") 
                << (interrupted ? "over-approximation " : "") << "size: "
                << curr_size << " out of " << init_size
                << (config.get_grp_mode() ? " groups" : " clauses")
                << " (" << 100.0*curr_size/init_size << "%)" 
                << endl;
    }
  }

  /* Tests the computed results: kicks off various testers for that.
   */
  void test_results(void)
  {
    if (pmd == 0)
      tool_abort("Got interrupted before any results were obtained.");
    MUSData& md = *pmd;

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
    }
  }

  /* Writes out the MU/GMU instance (or approximation) to the output file;
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
    oname += (config.get_grp_mode()) ? ".gcnf" : ".cnf";
    ofstream outf(oname.c_str());
    if (!outf)
      tool_abort("unable to open output file for writing");
    if (config.get_grp_mode()) 
      md.write_gcnf(outf);
    else
      md.write_cnf(outf);
  }

} // anonymous namespace

/*----------------------------------------------------------------------------*/
