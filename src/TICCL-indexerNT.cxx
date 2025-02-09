/*
  Copyright (c) 2006 - 2024
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of ticcltools

  ticcltools is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  ticcltools is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/ticcltools/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/
#include <unistd.h>
#include <set>
#include <limits>
#include <algorithm>
#include <vector>
#include <map>
#include <climits>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <fstream>
#include "config.h"
#ifdef HAVE_OPENMP
#include <omp.h>
#endif
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/Unicode.h"
#include "ticcutils/FileUtils.h"
#include "ticcl/ticcl_common.h"

#include "config.h"

using namespace std;
using namespace icu;
using ticcl::bitType;
set<bitType> follow_nums;

void usage( const string& name ){
  cerr << name << endl;
  cerr << "options: " << endl;
  cerr << "\t--hash=<anahash>\tname of the anagram hashfile. (produced by TICCL-anahash)" << endl;
  cerr << "\t--charconf=<charconf>\tname of the character confusion file. (produced by TICCL-lexstat)" << endl;
  cerr << "\t--foci=<focifile>\tname of the file produced by the --artifrq parameter of TICCL-anahash" << endl;
  cerr << "\t\t\t This file is used to limit the searchspace" << endl;
  cerr << "\t-o <outputfile>\t\tname for the outputfile. " << endl;
  cerr << "\t--confstats=<statsfile>\tcreate a list of confusion statistics"
       << endl;
  cerr << "\t--low=<low>\t skip entries from the anagram file shorter than "
       << endl;
  cerr << "\t\t\t'low' characters. (default = 5)" << endl;
  cerr << "\t--high=<high>\t skip entries from the anagram file longer than "
       << endl;
  cerr << "\t\t\t'high' characters. (default=35)" << endl;
  cerr << "\t-t <threads> or --threads <threads>\n\t\t\t Number of threads to run on." << endl;
  cerr << "\t\t\t If 'threads' has the value \"max\", the number of threads is set to a" << endl;
  cerr << "\t\t\t reasonable value. (OMP_NUM_TREADS - 2)" << endl;
  cerr << "\t-v\t\t run verbose " << endl;
  cerr << "\t-V or --version\t show version " << endl;
  cerr << "\t-h\t\t this message " << endl;
}

struct experiment {
  set<bitType>::const_iterator start;
  set<bitType>::const_iterator finish;
};

size_t init( vector<experiment>& exps,
	     const set<bitType>& hashes,
	     size_t threads ){
  exps.clear();
  size_t partsize = hashes.size() / threads;
  if ( partsize < 1 ){
    experiment e;
    e.start = hashes.begin();
    e.finish = hashes.end();
    exps.push_back( e );
    return 1;
  }
  auto s = hashes.begin();
  for ( size_t i=0; i < threads; ++i ){
    experiment e;
    e.start = s;
    for ( size_t j=0; j < partsize; ++j ){
      ++s;
    }
    e.finish = s;
    exps.push_back( e );
  }
  if ( s != hashes.end() ){
    exps[exps.size()-1].finish = hashes.end();
  }
  return threads;
}

void handle_exp( const experiment& exp,
		 size_t& count,
		 const set<bitType>& hashSet,
		 const set<bitType>& confSet,
		 map<bitType,set<bitType>>& result ){
  bitType max = *confSet.rbegin();
  auto it1 = exp.start;
  while ( it1 != exp.finish ){
#pragma omp critical
    {
      if ( ++count % 100 == 0 ){
	cout << ".";
	cout.flush();
	if ( count % 5000 == 0 ){
	  cout << endl << count << endl;;
	}
      }
    }
    auto it3 = hashSet.find( *it1 );
    if ( it3 != hashSet.end() ){
      set<bitType>::const_reverse_iterator it2( it3 );
      while ( it2 != hashSet.rend() ){
#pragma omp critical
	{
	  if ( follow_nums.find(*it2) != follow_nums.end() ){
	    cerr << "following: " << *it2 << endl;
	  }
	}
	bitType diff = *it1 - *it2;
	if ( diff > max ){
	  break;
	}
	if ( confSet.find( diff ) != confSet.end() ){
#pragma omp critical
	  {
	    result[diff].insert(*it2);
	    if ( follow_nums.find(diff) != follow_nums.end()
		 || follow_nums.find(*it2) != follow_nums.end() ){
	      cerr << "stored :" << diff << ":" << *it2 << endl;
	    }
	  }
	}
	++it2;
      }
      // it3 is already set at hashSet.find( *it1 );
      ++it3;
      while ( it3 != hashSet.end() ){
#pragma omp critical
	{
	  if ( follow_nums.find(*it1) != follow_nums.end() ){
	    cerr << "following: " << *it1 << endl;
	  }
	}
	bitType diff = *it3 - *it1;
	if ( diff > max ){
	  break;
	}
	if ( confSet.find( diff ) != confSet.end() ){
#pragma omp critical
	  {
	    result[diff].insert(*it1);
	    if ( follow_nums.find(diff) != follow_nums.end()
		 || follow_nums.find(*it1) != follow_nums.end() ){
	      cerr << "stored :" << diff << ":" << *it1 << endl;
	    }
	  }
	}
	++it3;
      }
    }
    ++it1;
  }
}

void output_result( ostream& os,
		    const map<bitType,set<bitType>>& result ){
  for ( auto const& [bt,bt_set] : result ){
    os << bt << "#";
    for ( const auto& it: bt_set ){
      os << it;
      if ( it != *bt_set.rbegin() ){
	os << ",";
      }
    }
    os << endl;
    os.flush();
  }
}

void output_confusions( ostream& csf,
			const map<bitType,set<bitType>>& result ){
  for ( auto const& [bt,bt_set] : result ){
    csf << bt << "#" << bt_set.size() << endl;
    csf.flush();
  }
}

int main( int argc, char **argv ){
  TiCC::CL_Options opts;
  try {
    opts.add_short_options( "vVho:t:" );
    opts.add_long_options( "charconf:,hash:,low:,high:,foci:,help,"
			   "version,threads:,confstats:,follow:" );
    opts.init( argc, argv );
  }
  catch( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage( argv[0] );
    exit( EXIT_FAILURE );
  }
  string progname = opts.prog_name();
  if ( opts.extract('h' ) || opts.extract("help") ){
    usage( progname );
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('V' ) || opts.extract("version") ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  if ( argc < 4	){
    usage( progname );
    exit(EXIT_FAILURE);
  }
  bool verbose = opts.extract( 'v' );
  string anahashFile;
  string confFile;
  string outFile;
  string fociFile;
  string confstats_file;
  int lowValue = 5;
  int highValue = 35;
  if ( !opts.extract( "hash", anahashFile ) ){
    cerr << "missing --hash option" << endl;
    exit( EXIT_FAILURE );
  }
  if ( !opts.extract( "charconf", confFile ) ){
    cerr << "missing --charconf option" << endl;
    exit( EXIT_FAILURE );
  }
  if ( !opts.extract( "foci", fociFile ) ){
    cerr << "missing --foci option" << endl;
    exit( EXIT_FAILURE );
  }
  opts.extract( "confstats", confstats_file );
  opts.extract( 'o', outFile );
  int num_threads = 1;
  string value = "1";
  if ( !opts.extract( 't', value ) ){
    opts.extract( "threads", value );
  }
#ifdef HAVE_OPENMP
  if ( TiCC::lowercase(value) == "max" ){
    num_threads = omp_get_max_threads() - 2;
  }
  else {
    if ( !TiCC::stringTo(value,num_threads) ) {
      cerr << "illegal value for -t (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
  }
#else
  if ( value != "1" ){
    cerr << "unable to set number of threads!.\nNo OpenMP support available!"
	 <<endl;
    exit(EXIT_FAILURE);
  }
#endif
  while ( opts.extract( "follow", value ) ){
    bitType follow_num;
    if ( !TiCC::stringTo(value,follow_num) ) {
      cerr << "illegal value for --follow (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
    follow_nums.insert( follow_num );
  }
  if ( opts.extract("low", value ) ){
    if ( !TiCC::stringTo(value,lowValue) ) {
      cerr << "illegal value for --low (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( opts.extract("high", value ) ){
    if ( !TiCC::stringTo(value,highValue) ) {
      cerr << "illegal value for --high (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( !opts.empty() ){
    cerr << "unsupported options : " << opts.toString() << endl;
    usage(progname);
    exit(EXIT_FAILURE);
  }

  if ( !TiCC::isFile( anahashFile ) ){
    cerr << "problem opening corpus word anagram hash file: "
	 << anahashFile << endl;
    exit(1);
  }
  if ( outFile.empty() ){
    outFile = anahashFile;
    string::size_type pos = outFile.rfind(".");
    if ( pos != string::npos ){
      outFile.resize(pos);
    }
    outFile += ".indexNT";
  }
  else if ( !TiCC::match_back( outFile, ".indexNT" ) ){
    outFile += ".indexNT";
  }

  ofstream *csf = 0;
  if ( !confstats_file.empty() ){
    csf = new ofstream( confstats_file );
    if ( !csf ){
      cerr << "problem opening outputfile: " << confstats_file << endl;
      exit(1);
    }
  }
  ofstream of( outFile );
  if ( !of ){
    cerr << "problem opening output file: " << outFile << endl;
    exit(1);
  }

  if ( !TiCC::isFile( confFile ) ){
    cerr << "problem opening character confusion anagram file: "
	 << confFile << endl;
    exit(1);
  }

  if ( !TiCC::isFile( fociFile ) ){
    cerr << "problem opening foci file: " << fociFile << endl;
    exit(1);
  }

  cout << "reading corpus word anagram hash values" << endl;
  ifstream cwav( anahashFile );
  size_t skipped = 0;
  set<bitType> hashSet = ticcl::read_anahash( cwav,
					      lowValue,
					      highValue,
					      skipped,
					      verbose );
  cout << "read " << hashSet.size() << " corpus word anagram values" << endl;
  cout << "skipped " << skipped << " out-of-band corpus word values" << endl;

  ifstream foc( fociFile );
  set<bitType> focSet = ticcl::read_bit_set( foc );
  cout << "read " << focSet.size() << " foci values" << endl;

  ifstream conf( confFile );
  set<bitType> confSet = ticcl::read_confusions( conf );
  cout << "read " << confSet.size()
       << " character confusion anagram values" << endl;

  vector<experiment> experiments;
  size_t expsize = init( experiments, focSet, num_threads );

  cout << "created " << expsize << " separate experiments" << endl;

#ifdef HAVE_OPENMP
  omp_set_num_threads( expsize );
  cout << "running on " << expsize << " threads." << endl;
#endif

  size_t count = 0;
  map<bitType,set<bitType> > result;
#pragma omp parallel for shared( experiments, count, result )
  for ( size_t i=0; i < expsize; ++i ){
    handle_exp( experiments[i], count, hashSet, confSet, result );
  }

  output_result( of, result );

  cout << "\nwrote indexes into: " << outFile << endl;
  if ( csf ){
    output_confusions( *csf, result );
    cout << "wrote confusion statistics into: " << confstats_file << endl;
    csf->close();
    delete csf;
  }
}
