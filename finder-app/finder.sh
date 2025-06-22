#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Rahul Sharma

filesdir=$1
searchstr=$2

if [ $# -ne 2 ]
then
        echo "Input Parameters missing. Must specify File directory and Search String as command argunments."
        exit 1 
fi
if [ -d $1 ]
then
    	fileslist=$( ls -1 $filesdir )
    	filesCount=$( ls -1 $filesdir | wc -l )
    	searchInFilesCount=$( grep -r "$searchstr" $filesdir 2>/dev/null |wc -l )
    	echo "The number of files are $filesCount and the number of matching lines are $searchInFilesCount"
else
    	echo $1 "The Input File directory doesn't exists."
    	exit 1
fi
