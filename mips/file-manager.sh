#!/bin/bash

process=`ps aux | grep dde-file-manager | grep -v grep`;

if [ "$process" == "" ]; then
	if [[ $# -ge 1 ]]; then
		dde-file-manager $1 $2
	else
		dde-file-manager $HOME
	fi
else
	if [[ $# -ge 1 ]]; then
		echo "{\"url\":\"$1\"}" |socat - $XDG_RUNTIME_DIR/dde-file-manager
	else
		echo "{\"url\":\"\~\"}" |socat - $XDG_RUNTIME_DIR/dde-file-manager
	fi
fi