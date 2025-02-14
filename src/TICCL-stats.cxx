/*
  Copyright (c) 2006 - 2024
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of ticcltools

  foliatools is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  foliatools is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/frog/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#include <cmath>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/XMLtools.h"
#include "ticcutils/Unicode.h"

#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;
using namespace	TiCC;

bool verbose = false;

void create_wf_list( const map<UnicodeString, unsigned int>& wc,
		     const string& filename, unsigned int totalIn,
		     unsigned int clip,
		     bool doperc ){
  unsigned int total = totalIn;
  ofstream os( filename );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  map<unsigned int, set<UnicodeString> > fws;
  for ( const auto& [word,freq] : wc ){
    if ( freq <= clip ){
      total -= freq;
    }
    else {
      fws[freq].insert( word );
    }
  }
  unsigned int sum=0;
  unsigned int types=0;
  auto wit = fws.rbegin();
  while ( wit != fws.rend() ){
    for( const auto& sit : wit->second ){
      sum += wit->first;
      os << sit << "\t" << wit->first;
      if ( doperc ){
	os << "\t" << sum << "\t" << 100 * double(sum)/total;
      }
      os << endl;
      ++types;
    }
    ++wit;
  }
#pragma omp critical
  {
    cout << "created WordFreq list '" << filename << "'" << endl
	 << "with " << total << " tokens and " << types
	 << " types. TTR= " << (double)types/total
	 << ", the angle is " << atan((double)types/total)*180/M_PI
	 << " degrees";
    if ( clip > 0 ){
      cout << "(" << totalIn - total << " of the original " << totalIn
	   << " words were clipped.)";
    }
    cout << endl;
  }
}

static void error_sink(void *mydata, const xmlError *error ){
  int *cnt = static_cast<int*>(mydata);
  if ( *cnt == 0 ){
    cerr << "\nXML-error: " << error->message << endl;
  }
  (*cnt)++;
}

bool is_emph( const UnicodeString& data ){
  return (data.length() < 2) && u_isalnum(data[0]);
}

size_t tel( const xmlNode *node, bool lowercase,
	    size_t ngram, const UnicodeString& sep,
	    map<UnicodeString, unsigned int>& wc,
	    set<UnicodeString>& emps ){
  vector<UnicodeString> buffer(ngram);
  size_t cnt = 0;
  size_t buf_cnt = 0;
  bool in_emph = false;
  UnicodeString emph_start;
  UnicodeString emph_word;
  const xmlNode *pnt = node->children;
  while ( pnt ){
    //    cerr << "bekijk label: " << (char*)pnt->name << endl;
    cnt += tel( pnt, lowercase, ngram, sep, wc, emps );
    if ( pnt->type == XML_TEXT_NODE ){
      UnicodeString line  = TiCC::UnicodeFromUTF8( TiCC::TextValue( pnt ) );
      //      cerr << "text: " << line << endl;
      vector<UnicodeString> v = TiCC::split( line );
      for ( const auto& word : v ){
	UnicodeString wrd = word;
	if ( lowercase ){
	  wrd.toLower();
	}
	if ( is_emph( wrd ) ){
	  if ( in_emph ){
	    emph_word += "_" + wrd;
	  }
	  else {
	    emph_start = wrd;
	    in_emph = true;
	  }
	}
	else {
	  if ( in_emph && !emph_word.isEmpty() ){
	    emps.insert( emph_start + emph_word );
	  }
	  in_emph = false;
	  emph_start.remove();
	  emph_word.remove();
	}
	buffer[buf_cnt++] = wrd;
	if ( buf_cnt == ngram ){
	  buf_cnt = ngram-1;
	  UnicodeString gram;
	  for( size_t i=0; i < ngram -1; ++i ){
	    gram += buffer[i] + sep;
	    buffer[i] = buffer[i+1];
	  }
	  gram+= buffer[ngram-1];
#pragma omp critical
	  {
	    ++wc[gram];
	  }
	  ++cnt;
	}
      }
    }
    pnt = pnt->next;
  }
  return cnt;
}

size_t word_xml_inventory( const string& docName,
			   bool lowercase,
			   size_t ngram,
			   const UnicodeString& sep,
			   map<UnicodeString,unsigned int>& wc,
			   set<UnicodeString>& emps ){
  xmlDoc *d = 0;
  int cnt = 0;
  xmlSetStructuredErrorFunc( &cnt, (xmlStructuredErrorFunc)error_sink );
  d = xmlReadFile( docName.c_str(), 0, XML_PARSE_NOBLANKS|XML_PARSE_HUGE );
  if ( !d || cnt > 0 ){
#pragma omp critical
    {
      cerr << "failed to load document '" << docName << "'" << endl;
    }
    return 0;
  }
  const xmlNode *root = xmlDocGetRootElement( d );
  size_t wordTotal = tel( root, lowercase, ngram, sep, wc, emps );
  xmlFree( d );
  return wordTotal;
}

size_t word_inventory( const string& docName,
		       bool lowercase,
		       size_t ngram,
		       const UnicodeString& sep,
		       map<UnicodeString,unsigned int>& wc,
		       set<UnicodeString>& emps,
		       bool dolines ){
  vector<UnicodeString> buffer(ngram);
  size_t buf_cnt = 0;
  size_t wordTotal = 0;
  ifstream is( docName );
  UnicodeString line;
  bool in_emph = false;
  UnicodeString emph_start;
  UnicodeString emph_word;
  while ( TiCC::getline( is, line ) ){
    if ( dolines ){
      buffer.clear();
      buf_cnt = 0;
    }
    vector<UnicodeString> v = TiCC::split( line );
    for ( const auto& word : v ){
      UnicodeString wrd = word;
      if ( lowercase ){
	wrd.toLower();
      }
      if ( is_emph( wrd ) ){
	if ( in_emph ){
	  emph_word += "_" + wrd;
	}
	else {
	  emph_start = wrd;
	  in_emph = true;
	}
      }
      else {
	if ( in_emph && !emph_word.isEmpty() ){
	  emps.insert( emph_start + emph_word );
	}
	in_emph = false;
	emph_start.remove();
	emph_word.remove();
      }
      buffer[buf_cnt++] = wrd;
      if ( buf_cnt == ngram ){
	buf_cnt = ngram-1;
	UnicodeString gram;
	for( size_t i=0; i < ngram -1; ++i ){
	  gram += buffer[i] + sep;
	  buffer[i] = buffer[i+1];
	}
	gram += buffer[ngram-1];
#pragma omp critical
	{
	  ++wc[gram];
	}
	++wordTotal;
      }
    }
  }
  return wordTotal;
}


void usage( const string& name ){
  cerr << "Usage: " << name << " [options] file/dir" << endl;
  cerr << "\t TICCL-stats will produce ngram statistics for a file, " << endl;
  cerr << "\t or a whole directory of files " << endl;
  cerr << "\t The output will be a 2 columned tab separated file, extension: *tsv " << endl;
  cerr << "\t--clip\t clipping factor. " << endl;
  cerr << "\t\t\t(entries with frequency <= this factor will be ignored). " << endl;
  cerr << "\t-p\t output percentages too. " << endl;
  cerr << "\t--lower\t Lowercase all words" << endl;
  cerr << "\t--ngram size\t create an ngram for 'size' ngrams (default 1-gram)" << endl;
  cerr << "\t--separator='sep' 	connect all n-grams with 'sep' (default is an underscore)" << endl;
  cerr << "\t--underscore\t Same as --separator='_'" << endl;
  cerr << "\t--hemp=<file>. Create a historical emphasis file. " << endl;
  cerr << "\t-t <threads> or --threads <threads> Number of threads to run on." << endl;
  cerr << "\t\t\t If 'threads' has the value \"max\", the number of threads is set to a" << endl;
  cerr << "\t\t\t reasonable value. (OMP_NUM_TREADS - 2)" << endl;
  cerr << "\t-n\t newlines delimit the input." << endl;
  cerr << "\t-v\t very verbose output." << endl;
  cerr << "\t-e expr\t specify the expression all input files should match with." << endl;
  cerr << "\t-o\t name of the output file(s) prefix." << endl;
  cerr << "\t-X\t the inputfiles are assumed to be XML. (all TEXT nodes are used)" << endl;
  cerr << "\t-R\t search the dirs recursively (when appropriate)." << endl;
  cerr << "\t-V or --version\t show version " << endl;
  cerr << "\t-h or --help \t this message." << endl;
}

int main( int argc, const char *argv[] ){
  CL_Options opts( "hnVvpe:t:o:RX", "clip:,lower,ngram:,underscore,separator:,hemp:,threads:" );
  try {
    opts.init(argc,argv);
  }
  catch( const OptionError& e ){
    cerr << e.what() << endl;
    usage(argv[0]);
    exit( EXIT_FAILURE );
  }
  string progname = opts.prog_name();
  if ( argc < 2 ){
    usage( progname );
    exit(EXIT_FAILURE);
  }
  int clip = 0;
  string expression;
  string outputPrefix;
  if ( opts.extract('V' ) || opts.extract("version") ){
    cerr << PACKAGE_STRING << endl;
    exit(EXIT_SUCCESS);
  }
  if ( opts.extract('h' ) || opts.extract("help") ){
    usage(progname);
    exit(EXIT_SUCCESS);
  }
  verbose = opts.extract( 'v' );
  bool dolines = opts.extract( 'n' );
  bool doXML = opts.extract( 'X' );
  if ( doXML && dolines ){
    cerr << "options -X and -n conflict!" << endl;
  }
  bool dopercentage = opts.extract('p');
  bool lowercase = opts.extract("lower");
  bool recursiveDirs = opts.extract( 'R' );
  UnicodeString sep = "_";
  string value;
  opts.extract( "separator", value );
  if ( !value.empty() ){
    sep = TiCC::UnicodeFromUTF8(value);
  }
  if ( opts.extract( "underscore" ) ){
    if ( sep != "_" ){
      cerr << "--separator and --underscore conflict!" << endl;
      exit(EXIT_FAILURE);
    }
    sep = "_";
  }
  string hempName;
  opts.extract("hemp", hempName );
  if ( !hempName.empty() ){
    ofstream out( hempName );
    if ( !out ){
      cerr << "unable to create historical emphasis file: " << hempName << endl;
    }
    exit(EXIT_FAILURE);
  }
  if ( !opts.extract( 'o', outputPrefix ) ){
    cerr << "an output filename prefix is required. (-o option) " << endl;
    exit(EXIT_FAILURE);
  }
  if ( opts.extract("clip", value ) ){
    if ( !stringTo(value, clip ) ){
      cerr << "illegal value for --clip (" << value << ")" << endl;
      exit(EXIT_FAILURE);
    }
  }
  size_t ngram = 1;
  if ( opts.extract("ngram", value ) ){
    if ( !stringTo(value, ngram )
	 || (ngram < 1) ){
      cerr << "illegal value for --ngram (" << value << ")" << endl;
      exit(EXIT_FAILURE);
    }
  }
  value = "1";
  if ( !opts.extract( 't', value ) ){
    opts.extract( "threads", value );
  }
#ifdef HAVE_OPENMP
  int numThreads=1;
  if ( TiCC::lowercase(value) == "max" ){
    numThreads = omp_get_max_threads() - 2;
    omp_set_num_threads( numThreads );
    cout << "running on " << numThreads << " threads." << endl;
  }
  else {
    if ( !TiCC::stringTo(value,numThreads) ) {
      cerr << "illegal value for -t (" << value << ")" << endl;
      exit( EXIT_FAILURE );
    }
    omp_set_num_threads( numThreads );
    cout << "running on " << numThreads << " threads." << endl;
  }
#else
  if ( value != "1" ){
    cerr << "unable to set number of threads!.\nNo OpenMP support available!"
	 <<endl;
    exit(EXIT_FAILURE);
  }
#endif
  opts.extract('e', expression );
  if ( !opts.empty() ){
    cerr << "unsupported options : " << opts.toString() << endl;
    usage(progname);
    exit(EXIT_FAILURE);
  }

  vector<string> massOpts = opts.getMassOpts();
  if ( massOpts.empty() ){
    cerr << "no file or dir specified!" << endl;
    exit(EXIT_FAILURE);
  }
  string name = massOpts[0];
  vector<string> fileNames = searchFilesMatch( name, expression, recursiveDirs );
  size_t toDo = fileNames.size();
  if ( toDo == 0 ){
    cerr << "no matching files found" << endl;
    exit(EXIT_SUCCESS);
  }

  string::size_type pos = outputPrefix.find( "." );
  if ( pos != string::npos && pos == outputPrefix.length()-1 ){
    // outputname ends with a '.', remove it
    outputPrefix.resize(pos);
  }
  pos = outputPrefix.find( "/" );
  if ( pos != string::npos && pos == outputPrefix.length()-1 ){
    // outputname ends with a /
    outputPrefix += "ticclstats";
  }

  string wf_filename = outputPrefix + ".wordfreqlist";
  string path = dirname( wf_filename ) + "/";
  if ( !TiCC::createPath( path ) ){
    cerr << "unable to create a path: " << path << endl;
    exit(EXIT_FAILURE);
  }
  string ng = toString(ngram);
  wf_filename += "." + ng + ".tsv";

  if ( toDo > 1 ){
    cout << "start processing of " << toDo << " files " << endl;
  }
  map<UnicodeString,unsigned int> wc;
  unsigned int wordTotal =0;

  set<UnicodeString> hemp;
#pragma omp parallel for shared(fileNames,wordTotal,wc,hemp)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    string docName = fileNames[fn];
    unsigned int word_count =  0;
    if ( doXML ){
      word_count = word_xml_inventory( docName, lowercase, ngram, sep, wc, hemp );
    }
    else {
      word_count = word_inventory( docName, lowercase, ngram, sep, wc, hemp, dolines );
    }
    wordTotal += word_count;
#pragma omp critical
    {
      cout << "Processed :" << docName << " with " << word_count << " words,"
	   << " still " << --toDo << " files to go." << endl;
    }
  }
  if ( toDo > 1 ){
    cout << "done processsing directory '" << name << "' in total "
	 << wordTotal << " words were found." << endl;
  }
  cout << "start calculating the results" << endl;
  if ( !hempName.empty() ){
    ofstream out( hempName );
    for( auto const& it : hemp ){
      out << it << endl;
    }
    cout << "historical emphasis stored in: " << hempName << endl;
  }
  create_wf_list( wc, wf_filename, wordTotal, clip, dopercentage );
  exit( EXIT_SUCCESS );
}
