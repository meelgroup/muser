/*----------------------------------------------------------------------------*\
 * File:        cnffmt.hh
 *
 * Description: Class definitions for CNF parser, based extensively on
 *              MiniSAT parser, but using the STL.
 *              NOTE: When linking, option -lz *must* be used
 *
 * Author:      jpms
 * 
 * Revision:    $Id: cnffmt.hh 73 2007-07-26 15:16:48Z jpms $.
 *
 *                                     Copyright (c) 2007, Joao Marques-Silva
\*----------------------------------------------------------------------------*/

#ifndef _CNFFMT_H
#define _CNFFMT_H 1

#include <ctime>
#include <cmath>
#include <unistd.h>
#include <signal.h>
#include <zlib.h>
#include <vector>

#include "globals.hh"
#include "id_manager.hh"
#include "fmtutils.hh"
#include "basic_clause.hh"
#include "basic_clset.hh"
#include "cl_id_manager.hh"

using namespace std;


//jpms:bc
/*----------------------------------------------------------------------------*\
 * DIMACS CNF Parser. (This borrows **extensively** from the MiniSAT parser)
\*----------------------------------------------------------------------------*/
//jpms:ec

template<class B>
static void read_cnf_clause(B& in, ULINT& mxid, vector<LINT>& lits) {
  LINT parsed_lit;
  lits.clear();
  for (;;){
    parsed_lit = parseInt(in);
    if (parsed_lit == 0) break;
    if (fabs(parsed_lit) > mxid) { mxid = fabs(parsed_lit); }
    lits.push_back(parsed_lit);
  }
}

using namespace std;

/** Template parameters: B - input stream, CSet - the clause set to populate
 */
template<class B, class CSet>
static void parse_cnf_file(B& in, IDManager& imgr, CSet& cldb) {
  ULINT mnid = 1;
  ULINT mxid = 1;
  ULINT clid = 0;       // clause's index in the input file
  vector<LINT> lits;
  for (;;){
    skipWhitespace(in);
    if (*in == EOF)
      break;
    else if (*in == 'c' || *in == 'p')
      skipLine(in);
    else {
      read_cnf_clause(in, mxid, lits);
      ++clid;
      // ANTON: for some applications its *essential* that clause id is the same
      // as the index of the clause in the input; if the input file contains 
      // dublicate clauses the automatic id will not be incremented, which in 
      // turn will mess up indexes of all clauses that follow; so catch up
      while (ClauseIdManager::Instance()->id() < clid) 
        ClauseIdManager::Instance()->new_id();
      BasicClause* ncl = cldb.create_clause(lits); // fheras: Automatic clause ID
      if (ncl != NULL)
        cldb.set_cl_grp_id(ncl, ncl->get_id());
    }
  }
  imgr.new_ids(mxid, mnid, mxid);  // Register used IDs
}

/** Template parameters: CSet -- the clause set to populate
 */
template<class CSet>
class CNFParserTmpl {
public:
  inline void load_cnf_file(gzFile input_stream,
			    IDManager& imgr, CSet& cldb) {
    StreamBuffer in(input_stream);
    parse_cnf_file(in, imgr, cldb); }
};
// definition of CNFParser, for backward compatibility
typedef CNFParserTmpl<BasicClauseSet> CNFParser;

#endif /* _CNFFMT_H */

/*----------------------------------------------------------------------------*/
