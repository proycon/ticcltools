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
#include <getopt.h>
#include <string>
#include <set>
#include <map>
#include <iostream>
#include <fstream>

#include "ticcutils/StringOps.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/Unicode.h"
#include "ticcl/ticcl_common.h"

#include "config.h"

using namespace	std;
using namespace icu;
using ticcl::bitType;

// globals
size_t artifreq = 0;
UnicodeString separator;
bool do_list = false;
bool do_merge = false;
bool do_ngrams = false;

void create_output( ostream& os,
		    const map<bitType, set<UnicodeString>>& anagrams ){
  for ( const auto& [val,str_set] : anagrams ){
    os << val << "~";
    for ( auto const&  s : str_set ){
      os << s;
      if ( s != *str_set.crbegin() )
	os << "#";
    }
    os << endl;
  }
  os << endl;
}

UnicodeString filter_tilde_hashtag( const UnicodeString& w ){
  // assume that we cannot break Unicode by replacing # or ~ by _
  UnicodeString result;
  for ( int i=0; i < w.length(); ++i ){
    if ( w[i] == '~' || w[i] == '#' ){
      result += '_';
    }
    else {
      result += w[i];
    }
  }
  return result;
}

void usage( const string& name ){
  cerr << "usage:" << name << " [options] <clean frequencyfile>" << endl;
  cerr << "\t" << name << " will read a wordfrequency list (in FoLiA-stats format) " << endl;
  cerr << "\t\t which is assumed to be 'clean', but may consist of n-grams with different arities." << endl;
  cerr << "\t\t The output will be an anagram hash file." << endl;
  cerr << "\t\t When a background corpus is specified, we also produce" << endl;
  cerr << "\t\t a new (merged) frequency file. " << endl;
  cerr << "\t\t When the --list option is specified, the inputfile is converted" << endl;
  cerr << "\t\t into a list of its words and their anagram hashes." << endl;
  cerr << "\t--list\t create a simple list of words and anagram hashes. (preserving order)" << endl;
  cerr << "\t--alph='file'\t name of the alphabet file" << endl;
  cerr << "\t--background='file'\t name of the background corpus" << endl;
  cerr << "\t--separator='char'\t The separator symbol for n-grams.(default '_')" << endl;
  cerr << "\t--clip=<clip> : cut off frequency of the alphabet. (freq 0 is NEVER clipped)" << endl;
  cerr << "\t-h or --help\t this message " << endl;
  cerr << "\t-o 'output_name' write output to file 'output_name'" << endl;
  cerr << "\t--artifrq='value': if value > 0, create a separate list of anagram" << endl;
  cerr << "\t\t values that have a lexical frequency < 'artifrq'. (default=0)" << endl;
  cerr << "\t\t for n-grams, only those n-grams are written where at least one" << endl;
  cerr << "\t\t of the composing parts does not have the lexical frequency artifrq. " << endl;
  cerr << "\t--ngrams When the frequency file contains n-grams. (not necessary of equal arity)" << endl;
  cerr << "\t\t we split them into 1-grams and do a frequency lookup per part for the artifreq value." << endl;
  cerr << "\t-V or --version\t show version " << endl;
  cerr << "\t-v\t verbose (not used yet) " << endl;
}

void read_backgound( istream& is,
		     map<bitType, set<UnicodeString>>& anagrams,
		     map<UnicodeString,bitType>& merged,
		     const map<UChar,bitType>& alphabet ){
  UnicodeString line;
  while ( TiCC::getline( is, line ) ){
    vector<UnicodeString> v = TiCC::split_at( line, "\t" );
    if ( ! ( v.size() == 1 || v.size() == 2 ) ){
      cerr << "background file in wrong format!" << endl;
      cerr << "offending line: " << line << endl;
      exit(EXIT_FAILURE);
    }
    UnicodeString word = filter_tilde_hashtag( v[0] );
    bitType h = ticcl::hash( word, alphabet );
    anagrams[h].insert( word );
    bitType freq = 1;
    if ( v.size() == 2 ){
      freq = TiCC::stringTo<bitType>( v[1] );
    }
    merged[v[0]] += freq;
  }
}

void read_data( istream& is,
		map<bitType, set<UnicodeString>>& anagrams,
		map<UnicodeString,bitType>& merged,
		map<UnicodeString,bitType>& freq_list,
		const map<UChar,bitType>& alphabet,
		ostream& os ){
  UnicodeString line;
  while ( TiCC::getline( is, line ) ){
    // we build a frequency list
    vector<UnicodeString> v = TiCC::split_at( line, "\t" );
    if ( !( v.size() == 1 || v.size() == 2 ) ){
      cerr << "frequency file in wrong format!" << endl;
      cerr << "offending line: " << line << endl;
      exit(EXIT_FAILURE);
    }
    UnicodeString word = filter_tilde_hashtag( v[0] );
    bitType h = ticcl::hash( word, alphabet );
    if ( do_list ){
      os << v[0] << "\t" << h << endl;
    }
    else {
      anagrams[h].insert( word );
      bitType freq = 1;
      if ( v.size() == 2 ){
	freq = TiCC::stringTo<bitType>( v[1] );
      }
      freq_list[word] = freq;
      if ( do_merge && artifreq > 0  ){
	merged[v[0]] = freq;
      }
    }
  }
}

map<bitType, set<UnicodeString>>
extract_foci( const map<UnicodeString,bitType>& freq_list,
	      const map<UChar,bitType>& alphabet ){
  map<bitType, set<UnicodeString>> foci;
  for ( const auto& [val,freq] : freq_list ){
    UnicodeString word = val;
    bitType h = ticcl::hash( word, alphabet );
    if ( do_ngrams ){
      vector<UnicodeString> parts = TiCC::split_at( word, separator );
      if ( parts.size() > 0 ){
	// we have an -n-gram
	bool accept = false;
	// we split the ngram to see if it is worth adding it to
	// the foci list.
	//    - NOT if no part is in the input
	//    - NOT if all parts are know words.
	for ( auto const& part: parts ){
	  const auto u_it = freq_list.find( part );
	  if ( u_it != freq_list.end()
	       && u_it->second < artifreq ){
	    // so this part IS present in the input, but not in the background
	    UnicodeString l_part = part;
	    l_part.toLower();
	    const auto l_it = freq_list.find(l_part);
	    if ( l_it == freq_list.end()
		 || l_it->second < artifreq ){
	      // the lowercase part is NOT present OR NOT the background
	      accept = true;
	    }
	  }
	}
	if ( accept ){
	  word.toLower();
	  foci[h].insert( word );
	}
      }
    }
    else {
      if ( freq < artifreq ){
	word.toLower();
	const auto l_it = freq_list.find(word);
	if ( l_it == freq_list.end()
	     || l_it->second < artifreq ){
	  foci[h].insert(word);
	}
      }
    }
  }
  return foci;
}

int main( int argc, const char *argv[] ){
  TiCC::CL_Options opts;
  try {
    opts.add_short_options( "vVho:" );
    opts.add_long_options( "alph:,background:,artifrq:,clip:,help,version,ngrams,list,separator:" );
    opts.init( argc, argv );
  }
  catch( TiCC::OptionError& e ){
    cerr << e.what() << endl;
    usage( argv[0] );
    exit( EXIT_FAILURE );
  }
  string progname = opts.prog_name();
  if ( argc < 2	){
    usage( progname );
    exit(EXIT_FAILURE);
  }
  string alphafile;
  string backfile;
  int clip = 0;
  if ( opts.extract('h' ) || opts.extract("help") ){
    usage( progname );
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('V') || opts.extract("version") ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  bool verbose = opts.extract( 'v' );
  opts.extract( "alph", alphafile );
  opts.extract( "background", backfile );
  opts.extract( "separator", separator );
  if ( separator.isEmpty() ){
    separator = ticcl::US_SEPARATOR;
  }
  do_list = opts.extract( "list" );
  string value;
  if ( opts.extract( "clip", value ) ){
    if ( !TiCC::stringTo(value,clip) ) {
      cerr << "illegal value for --clip (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
  }
  if ( opts.extract( "artifrq", value ) ){
    if ( !TiCC::stringTo(value,artifreq) ) {
      cerr << "illegal value for --artifrq (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
  }
  do_ngrams = opts.extract( "ngrams" );
  string out_file_name;
  opts.extract( "o", out_file_name );
  if ( !opts.empty() ){
    cerr << "unsupported options : " << opts.toString() << endl;
    usage(progname);
    exit(EXIT_FAILURE);
  }
  if ( verbose ){
    cout << "artifrq= " << artifreq << endl;
  }
  vector<string> fileNames = opts.getMassOpts();
  if ( fileNames.empty() ){
    cerr << "missing input file" << endl;
    exit(EXIT_FAILURE);
  }
  else if ( fileNames.size() > 1 ){
    cerr << "only one input file is possible." << endl;
    cerr << "but found: " << endl;
    for ( const auto& s : fileNames ){
      cerr << s << endl;
    }
    exit(EXIT_FAILURE);
  }
  string file_name = fileNames[0];
  if ( !TiCC::isFile( file_name) ){
    cerr << "unable to open corpus frequency file: " << file_name << endl;
    exit(EXIT_FAILURE);
  }
  if ( alphafile.empty() ){
    cerr << "We need an alphabet file!" << endl;
    exit(EXIT_FAILURE);
  }
  if ( !TiCC::isFile(alphafile) ){
    cerr << "unable to open alphabet file: " << alphafile << endl;
    exit(EXIT_FAILURE);
  }
  if ( out_file_name.empty() ){
    out_file_name = file_name;
    if ( do_list ){
      out_file_name += ".list";
    }
    else {
      out_file_name += ".anahash";
    }
  }
  else if ( do_list ){
    // assure .list suffix
    if ( !TiCC::match_back( out_file_name, ".list" ) ){
      out_file_name += ".list";
    }
  }
  else {
    // assure .anahash suffix
    if ( !TiCC::match_back( out_file_name, ".anahash" ) ){
      out_file_name += ".anahash";
    }
  }

  map<UChar,bitType> alphabet;
  cout << "reading alphabet file: " << alphafile << endl;
  ifstream as( alphafile );
  if ( !ticcl::fillAlphabet( as, alphabet, clip ) ){
    cerr << "serious problems reading alphabet file: " << alphafile << endl;
    exit(EXIT_FAILURE);
  }
  cout << "finished reading alphabet. (" << alphabet.size() << " characters)"
       << endl;
  string foci_file_name = file_name;
  if ( do_list ){
    if ( artifreq > 0 ){
      cerr << "option --artifrq not supported for --list" << endl;
      exit( EXIT_FAILURE);
    }
    if ( !backfile.empty() ){
      cerr << "option --background not supported for --list" << endl;
      exit( EXIT_FAILURE);
    }
    if ( !TiCC::createPath( out_file_name ) ){
      cerr << "unable to open output file: " << out_file_name << endl;
      exit(EXIT_FAILURE);
    }
  }
  else {
    if ( !TiCC::createPath( out_file_name ) ){
      cerr << "unable to open output file: " << out_file_name << endl;
      exit(EXIT_FAILURE);
    }
    foci_file_name = file_name + ".corpusfoci";
    if ( artifreq > 0 ){
      if ( !TiCC::createPath( foci_file_name ) ){
	cerr << "unable to open foci file: " << foci_file_name << endl;
	exit(EXIT_FAILURE);
      }
    }
    if ( !backfile.empty() ){
      if ( !TiCC::isFile( backfile) ){
	cerr << "unable to open background frequency file: " << backfile << endl;
	exit(EXIT_FAILURE);
      }
      do_merge = true;
    }
  }
  map<UnicodeString,bitType> merged;
  map<UnicodeString,bitType> freq_list;
  map<bitType, set<UnicodeString>> anagrams;
  cout << "start hashing from the corpus frequency file: " << file_name << endl;
  ifstream is( file_name );
  ofstream out_stream( out_file_name );
  read_data( is,
	     anagrams,
	     merged,
	     freq_list,
	     alphabet,
	     out_stream );

  if ( do_list ){
    cout << "created a list file: " << out_file_name << endl;
    exit( EXIT_SUCCESS );
  }
  if ( artifreq > 0 ){ // so NOT when creating a simple list!
    auto foci = extract_foci( freq_list,
			      alphabet );
    cout << "generating foci file: " << foci_file_name << " with " << foci.size() << " entries" << endl;
    ofstream fos( foci_file_name );
    create_output( fos, foci );
  }
  if ( do_merge ){
    cerr << "merge background corpus: " << backfile << endl;
    ifstream bs( backfile );
    read_backgound( bs, anagrams, merged, alphabet );
    string merge_file_name = file_name + ".merged";
    ofstream ms( merge_file_name );
    for ( const auto& [word,freq] : merged ){
      ms << word << "\t" << freq << endl;
    }
    cerr << "stored merged corpus in " << merge_file_name << endl;

  }

  cout << "generating output file: " << out_file_name << endl;
  create_output( out_stream, anagrams );
  cout << "done!" << endl;
}
