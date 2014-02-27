/*----------------------------------------------------------------------------*\
 * File:        muser2_impl.cc
 *
 * Description: Implementation of the MUS/MES extraction API implementation class.
 *
 * Author:      antonb
 * 
 * Notes:
 *
 *                                              Copyright (c) 2012, Anton Belov
\*----------------------------------------------------------------------------*/

#include "muser2_impl.hh"
#include <iterator>

using namespace std;

#define DBG(x) x

namespace {     // local declarations ...

}

/** Constructor */
muser2::muser2_impl::muser2_impl(void)
{
  DBG(cout << "= muser2::constructor" << endl;);
  // prepare default parameters
  // TODO: all the configuration parameters passable through API, so that users
  // could configure SAT solvers, algorithms, etc.
  config.set_grp_mode();
  config.set_sat_solver("glucoses");
  config.set_refine_clset_mode();
  config.set_rmr_mode();
}

/** Destructor */
muser2::muser2_impl::~muser2_impl(void)
{
  DBG(cout << "= muser2::destructor" << endl;);
}

/** Initializes all internal data-structures */
void muser2::muser2_impl::init_all(void)
{
  DBG(string cfg; config.get_cfgstr(cfg);
      cout << "= muser2::init_all, configuration string: " << cfg << endl;);
  _pgset = new BasicGroupSet(config);
}

/** Resets all internal data-structures */
void muser2::muser2_impl::reset_all(void)
{
    //INIT ALEX Remove all clauses
  vector<BasicClause*>::iterator it_sapos = cl_savec.begin();
  vector<BasicClause*>::iterator it_saend = cl_savec.end();
  for ( ; it_sapos != it_saend; ++it_sapos){
    BasicClause* sa_cl = *it_sapos;
    //cout << "deleting clause: " << sa_cl << endl;
    _pgset->destroy_clause(sa_cl);
  }
  cl_savec.clear();
  //END ALEX
  
  DBG(cout << "= muser2::reset_all" << endl;);
  delete _pgset;
}

/** Prepares extractor for the run */
void muser2::muser2_impl::init_run(void)
{
  DBG(string cfg; config.get_cfgstr(cfg);
      cout << "= muser2::init_run, configuration string: " << cfg << endl;);
  _pmd = new MUSData(*_pgset);
  _imgr.reg_ids(_pgset->max_var()); // FIXME: this will break in incremental mode !
  _gmus_gids.clear();
  _init_gsize = _pgset->gsize() - _pgset->has_g0();
}

/** Clears up all data-structures used for the run */
void muser2::muser2_impl::reset_run(void)
{
  DBG(cout << "= muser2::reset_run" << endl;);
  delete _pmd;
}

/** Tests the current group-set for satisfiability.
 * @return SAT competition code: 0 - UNKNOWN, 10 - SAT, 20 - UNSAT
 */
unsigned muser2::muser2_impl::test_sat(void)
{
  DBG(cout << "= muser2::test_sat, checking for satisfiability ..." << endl;);
  SATChecker schecker(_imgr, config);
  CheckUnsat cu(*_pmd);
  if (schecker.process(cu) && cu.completed())
    return cu.is_unsat() ? 20 : 10;
  else
    return 0;
}

/** Compute a group-MUS of the current group-set.
 */
int muser2::muser2_impl::compute_gmus(void)
{
  DBG(cout << "= muser2::compute_gmus, computing ..." << endl;);
  MUSExtractor mex(_imgr, config);
  mex.set_cpu_time_limit(_cpu_limit);
  mex.set_iter_limit(_iter_limit);
  ComputeMUS cm(*_pmd);
  if (mex.process(cm) && cm.completed()) {
    for_each(_pgset->gbegin(), _pgset->gend(), [&](GID gid) {
        if (gid && !_pmd->r(gid)) { _gmus_gids.push_back(gid); }
      });
    bool approx = (_pmd->nec_gids().size() - _pmd->nec(0)) != _gmus_gids.size();
    if (_verb >= 1) {
      cout_pref << "muser2 finished in " << mex.cpu_time() << " sec."
                << " init_size: " << _init_gsize
                << " GMUS_size: " << _gmus_gids.size() 
                << " exact: " << !approx
                << " SAT_calls: " << mex.sat_calls() 
                << " rotated: " << mex.rot_groups() 
                << " refined: " << mex.ref_groups()
                << endl;
    }
    DBG(cout << "muser2 core : { ";
        for (GID gid : _gmus_gids) { cout << gid << " "; }
        cout << "}" << endl;);
    return (approx) ? 0 : 20;
  }
  return -1;
}


/** Add a clause to the group-set
 */
muser2::gid muser2::muser2_impl::add_clause(const int* first, const int* last, muser2::gid gid)
{
  for (const int* f = first; f < last+1; ++f) cout << *f << " ";
  cout << "0" << endl;
  vector<LINT> lits(first, last + 1);
  for (LINT l : lits) { cout << l << " "; }
  cout << "0" << endl;
  BasicClause* cl = _pgset->create_clause(lits);
  //INIT ALEX
  cl_savec.push_back(cl);
  //END ALEX
  if (cl->get_grp_id() == gid_Undef) {
    if (gid == gid_Undef) { gid = _pgset->max_gid() + 1; }
    _pgset->set_cl_grp_id(cl, (GID)gid);     
    DBG(cout << "= muser2::add_clause: new clause ";);
  } DBG(else { cout << "= muser2::add_clause: existing clause "; });
  // TODO: if the clause already exists, it needs to be deleted !
  DBG(cout << "{" << cl->get_grp_id() << "} " << *cl << endl;);
  return (muser2::gid)cl->get_grp_id();
}

/*----------------------------------------------------------------------------*/
