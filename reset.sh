#!/bin/bash

SERVER='192.168.0.148'
AT_PORT='9200'
NC_PORT='4200'
INTERVAL='1'

LOG='log.log'

#echo "" > $LOG

#This is the first parser command, later "pong" will simply
#become "parser" this currently only prevents ping timeouts


function reset {
	#Connect to the server, Identify, and join channel
	echo -e "ATP\n";

	}
function input {
	#Connect to the server, Identify, and join channel
	echo -e commands.txt;

	}

function AT_reset {
	reset | nc -i $INTERVAL $SERVER $AT_PORT >> $LOG;
}

function NC_commands {
	input | ../sdmsh/sdmsh $SERVER $NC_PORT >> $LOG;
}

AT_reset
