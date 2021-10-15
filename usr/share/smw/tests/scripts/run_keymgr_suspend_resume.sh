#! /bin/bash

config_file="/usr/share/smw/tests/config/"
status_file_prefix="/usr/share/smw/tests/"
definition_file_prefix="/usr/share/smw/tests/test_definition/"
definition_file_suffix=".json"
status_file_suffix=".txt"
ids_file_suffix="_ids.json"
res=0

if [ $# -ne 3 ]
then
	echo "This script needs 3 arguments:"
	echo " - 1st: SMW configuration file name. Must be located in: $config_file directory"
	echo " - 2nd: 1st test definition file path"
	echo " - 3rd: 2nd test definition file path"
	echo "This test:"
	echo " - runs the 1st test (1st test definition file)"
	echo " - runs suspend resume script"
	echo " - runs the 2nd test (2nd test definition file)"
	exit 1
fi

config_file+=$1

test1_name=$2
test1_name=${test1_name#$definition_file_prefix}
test1_name=${test1_name%$definition_file_suffix}

test1_status_file=$status_file_prefix
test1_status_file+=$test1_name
test1_status_file+=$status_file_suffix

main_test_name=$test1_name
main_test_name=${main_test_name%%.*}
main_test_status_file=$main_test_name
main_test_status_file+=$status_file_suffix

test2_name=$3
test2_name=${test2_name#$definition_file_prefix}
test2_name=${test2_name%$definition_file_suffix}

test2_status_file=$status_file_prefix
test2_status_file+=$test2_name
test2_status_file+=$status_file_suffix

ids_file_name=$main_test_name
ids_file_name+=$ids_file_suffix

export SMW_CONFIG_FILE=$config_file

smwtest -d $2
res=$?
if [ $res -ne 0 ]
then
	echo "$2 test failed"
	exit $res
fi

/usr/share/smw/tests/scripts/suspend_resume.sh
res=$?
if [ $res -ne 0 ]
then
	echo "Suspend resume script failed"
	exit $res
fi

smwtest -d $3
res=$?
if [ $res -ne 0 ]
then
	echo "$3 test failed"
	exit $res
fi

#
# Concatenate test1 and test2 status files into a main status file
#
cat $test1_status_file $test2_status_file > $main_test_status_file
res=$?
if [ $res -ne 0 ]
then
	echo "Internal error: 'cat' command failed"
	exit $res
fi

#
# Delete test1 and test2 status files and key ids file
#
rm $test1_status_file $test2_status_file $ids_file_name
res=$?
if [ $res -ne 0 ]
then
	echo "Internal error: 'rm' command failed"
fi

exit $res
