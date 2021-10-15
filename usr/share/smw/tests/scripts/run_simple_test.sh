#! /bin/bash

config_file="/usr/share/smw/tests/config"

if [ $# -ne 2 ]
then
	echo "This script needs 2 arguments:"
	echo " - 1st: SMW configuration file name. Must be located in: $config_file directory"
	echo " - 2nd: Test definition file path."
	exit 1
fi

config_file+="/$1"
export SMW_CONFIG_FILE=$config_file
smwtest -d $2
