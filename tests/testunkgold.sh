#!/bin/bash

extra_option=$1

bindir=/home/sloot/usr/local/bin

if [ ! -d $bindir ]
then
   bindir=/exp/sloot/usr/local/bin
   if [ ! -d $bindir ]
   then
       echo "cannot find executables "
       exit
   fi
fi

outdir=TESTRESULTS
refdir=TESTDATA
testdir=TESTDATA
datadir=DATA

echo start TICLL-unk

$bindir/TICCL-unk --background $datadir/INLandAspell.corpus --artifrq 100000000 --acro --filter=unk.rules $extra_option -o$outdir/gold1 $testdir/gold.tsv

if [ $? -ne 0 ]
then
    echo "failed in TICCL-unk"
    exit
fi
echo "checking UNK results...."
diff $outdir/gold1.punct $refdir/gold1.punct > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK punct results"
    echo "using: diff $outdir/gold1.punct $refdir/gold1.punct"
    exit
fi
diff $outdir/gold1.unk $refdir/gold1.unk > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK unk results"
    echo "using: diff $outdir/gold1.unk $refdir/gold1.unk"
    exit
fi
diff $outdir/gold1.clean $refdir/gold1.clean > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK clean results"
    echo "using: diff $outdir/gold1.clean $refdir/gold1.clean"
    exit
fi
diff $outdir/gold1.fore.clean $refdir/gold1.fore.clean > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK fore.clean results"
    echo "using: diff $outdir/gold1.fore.clean $refdir/gold1.fore.clean"
    exit
fi
diff $outdir/gold1.acro $refdir/gold1.acro > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK acro results"
    echo "using: diff $outdir/gold1.acro $refdir/gold1.acro"
    exit
fi

echo start TWEEDE TICLL-unk run

$bindir/TICCL-unk --alph $datadir/nld.aspell.dict.clip20.lc.chars --acro -o$outdir/gold2 --hemp=$testdir/hemp.data $extra_option $testdir/gold.tsv

if [ $? -ne 0 ]
then
    echo "failed in TICCL-unk"
    exit
fi
echo "checking UNK results...."
diff $outdir/gold2.punct $refdir/gold2.punct > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK punct2 results"
    echo "using: diff $outdir/gold2.punct $refdir/gold2.punct"
    exit
fi
diff $outdir/gold2.unk $refdir/gold2.unk > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK unk2 results"
    echo "using: diff $outdir/gold2.unk $refdir/gold2.unk"
    exit
fi
diff $outdir/gold2.clean $refdir/gold2.clean > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK clean2 results"
    echo "using: diff $outdir/gold2.clean $refdir/gold2.clean"
    exit
fi
diff $outdir/gold2.acro $refdir/gold2.acro > /dev/null 2>&1
if [ $? -ne 0 ]
then
    echo "differences in Ticcl-UNK acro2 results"
    echo "using: diff $outdir/gold2.acro $refdir/gold2.acro"
    exit
fi


echo OK!
