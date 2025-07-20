#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Rahul Sharma

#!/bin/sh
writefile=$1
writestr=$2

if [ $# -ne 2 ]
    then
        echo "Input Parameters missing. Must specify File directory and Search String as command argunments."
        exit 1 
fi
mkdir -p $( dirname $writefile)
if [ $? -ne 0 ]
    then
        echo "File Path can't be created!"
	exit 1
fi
touch $writefile 
if [ -w $writefile ]
    then
        if [ $? -ne 0 ] || [ -d $writefile ]
        then
            echo "File $writefile can't be created!"
            exit 1
        fi
    echo $writestr > $writefile
    else
        echo "$writefile is not writable"
        exit 1
fi
