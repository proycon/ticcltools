#!/bin/bash

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

outdir=OUT
refdir=OUTreference/BOOK
datadir=DATA


echo "start TICLL-chain"

$bindir/TICCL-chain -v $refdir/book.ranked --alph $datadir/nld.aspell.dict.clip20.lc.chars -o $outdir/book.chained # --nounk

if [ $? -ne 0 ]
then
    echo failed INSIDE TICLL-chain
    exit
fi


echo "checking chain results...."
diff $outdir/book.chained.debug $refdir/rank.chained.debug >& /dev/null
if [ $? -ne 0 ]
then
    echo "differences in TICLL-chain results"
    echo "using diff $outdir/book.chained.debug $refdir/rank.chained.debug"
    exit
fi

diff $outdir/book.chained $refdir/rank.chained >& /dev/null
if [ $? -ne 0 ]
then
    echo "differences in TICLL-chain results"
    echo "using diff $outdir/book.chained $refdir/rank.chained"
    exit
fi

echo "and now caseless"

$bindir/TICCL-chain --caseless --alph $datadir/nld.aspell.dict.clip20.lc.chars -o $outdir/caseless.ranked.chained $refdir/book.ranked # --nounk

if [ $? -ne 0 ]
then
    echo failed inside TICLL-chain
    exit
fi

echo "checking caseless chain results...."

diff $outdir/caseless.ranked.chained $refdir/caseless.rank.chained >& /dev/null
if [ $? -ne 0 ]
then
    echo "differences in TICLL-chain results"
    echo "using diff $outdir/caseless.ranked.chained $refdir/caseless.rank.chained"
    exit
else
    echo OK!
fi
