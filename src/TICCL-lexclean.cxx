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

#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <iomanip>
#include <iostream>
#include <fstream>

#include "ticcutils/CommandLine.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Unicode.h"

#include "config.h"

using namespace	std;
using namespace	icu;

void create_wf_list( const map<UnicodeString, unsigned int>& wc,
		     const string& filename, unsigned int totalIn,
		     bool doperc ){
  unsigned int total = totalIn;
  ofstream os( filename );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  map<unsigned int, set<UnicodeString> > wf;
  for ( const auto& [word,freq] : wc ){
    wf[freq].insert(word);
  }
  unsigned int sum=0;
  auto wit = wf.rbegin();
  while ( wit != wf.rend() ){
    if ( wit->first == 0 ){
      for ( const auto& s : wit->second ){
	os << s << endl;
	++total;
      }
    }
    else {
      for ( const auto& s : wit->second ){
	sum += wit->first;
	os << s << "\t" << wit->first;
	if ( doperc ){
	  os << "\t" << sum << "\t" << std::setprecision(8) << 100 * double(sum)/total;
	}
	os << endl;
      }
    }
    ++wit;
  }
  cout << "created cleaned list '" << filename << "'" << endl;
  cout << "with " << total << " words." << endl;
}

void dump_quarantine( const string& filename,
		      const map<UnicodeString, unsigned int>& qw ){
  ofstream os( filename );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  for ( const auto& [word,freq] : qw ){
    os << word;
    if ( freq > 0 ){
      os << "\t" << freq;
    }
    os << endl;
  }
  cout << "created quarantine list '" << filename << "'" << endl;
  cout << "with " << qw.size() << " items. " << endl;
}

bool isClean( const UnicodeString& us, const set<UChar>& alp, bool reverse ){
  //  cerr << "check " << us << endl;
  for ( int i=0; i < us.length(); ++i ){
    //    cerr << "check " << us[i] << endl;
    if ( alp.find( us[i] ) == alp.end() ){
      if ( reverse ){
	continue;
      }
      //      cerr << "rejected" << endl;
      return false;
    }
    if ( reverse ){
      //      cerr << "rejected" << endl;
      return false;
    }
  }
  //  cerr << "OK" << endl;
  return true;
}

bool fillAlpha( const string& file, set<UChar>& alphabet ){
  UnicodeString line;
  ifstream is( file );
  while ( TiCC::getline( is, line ) ){
    if ( line.length() == 0 || line[0] == '#' ){
      continue;
    }
    vector<UnicodeString> v = TiCC::split( line );
    UnicodeString us = v[0];
    us.toLower();
    alphabet.insert( us[0] );
    us.toUpper();
    alphabet.insert( us[0] );
    // for now, we don't use the other fields
  }
  return true;
}


void usage(){
  cerr << "Usage: [options] file/dir" << endl;
  cerr << "\t TICCL-lexclean will clean 1, 2 or 4 columned (lexicon) files" << endl;
  cerr << "\t The output will be a 1, 2 or 4 columned tab separated file, extension: .cleaned " << endl;
  cerr << "\t\t (4 columns when -p is specified)" << endl;
  cerr << "\t 'dirty' words are written to a .dirty file." << endl;
  cerr << "\t-a\t alphabet file. With characters to accept" << endl;
  cerr << "\t-x\t unwanted alphabet file. With characters to reject" << endl;
  cerr << "\t-p\t output percentages too. " << endl;
  cerr << "\t-t\t assume a POS tagged input" << endl;
  cerr << "\t-h or --help\t this message" << endl;
  cerr << "\t-V or --version\t show version " << endl;
}

int main( int argc, const char *argv[] ){
  TiCC::CL_Options opts( "hVpa:x:t", "help,version" );
  try {
    opts.init(argc,argv);
  }
  catch( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage();
    exit( EXIT_FAILURE );
  }

  bool dopercentage = false;
  bool postagged = false;
  string value;
  string alpha;
  string no_alpha;
  bool reverse = false;
  if ( opts.extract('V', value ) || opts.extract("version",value) ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('h', value ) || opts.extract("help", value ) ){
    usage();
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('a', value ) ){
    alpha = value;
  }
  if ( opts.extract('x', value ) ){
    no_alpha = value;
    reverse = true;
  }
  if ( !alpha.empty() && !no_alpha.empty() ){
    cerr << "may not combine -a and -x" << endl;
    exit( EXIT_FAILURE );
  }
  if ( opts.extract('p', value ) ){
    dopercentage = true;
  }
  if ( opts.extract('t', value ) ){
    postagged = true;
  }
  if ( !opts.empty() ){
    usage();
    exit(EXIT_FAILURE);
  }

  set<UChar> alphabet;
  if ( !alpha.empty() ){
    fillAlpha( alpha, alphabet );
    cerr << "read alphabet file with " << alphabet.size()
	 << " characters" << endl;
  }
  if ( !no_alpha.empty() ){
    fillAlpha( no_alpha, alphabet );
    cerr << "read EXCLUDE alphabet file with " << alphabet.size()
	 << " characters" << endl;
  }

  vector<string> fileNames = opts.getMassOpts();

  size_t toDo = fileNames.size();
  if ( toDo == 0 ){
    cerr << "no matching files found" << endl;
    usage();
    exit(EXIT_SUCCESS);
  }
  map<UnicodeString,unsigned int> wc;
  map<UnicodeString,unsigned int> qw;
  for ( const auto& docName : fileNames ){
    ifstream is( docName );
    UnicodeString line;
    unsigned int word_total = 0;
    while ( TiCC::getline( is, line ) ){
      vector<UnicodeString> vec = TiCC::split_at( line, "\t" );
      size_t num = vec.size();
      if ( num == 1 ){
	if ( isClean( vec[0], alphabet, reverse ) ){
	  wc[vec[0]] = 0;
	}
	else {
	  qw[vec[0]] = 0;
	}
      }
      else if ( num == 2 || num == 4 ){
	UnicodeString val = vec[0];
	if ( postagged ){
	  vector<UnicodeString> v2 = TiCC::split( val );
	  if ( v2.size() > 1 ){
	    val = v2[0];
	  }
	  else {
	    cerr << "pos tagged files need a space separated value in the first column" << endl;
	    exit(EXIT_FAILURE);
	  }
	}
	unsigned int freq = TiCC::stringTo<unsigned int>( vec[1] );
	if ( isClean( val, alphabet, reverse ) ){
	  wc[vec[0]] = freq;
	  word_total += freq;
	}
	else {
	  qw[vec[0]] = freq;
	}
      }
      else {
	cerr << "unexpected line: '" << line << "' in " << docName << endl;
	continue;
      }
    }
    string outname = docName + ".cleaned";
    create_wf_list( wc, outname, word_total, dopercentage );
    outname = docName + ".dirty";
    dump_quarantine( outname, qw );
  }
}
