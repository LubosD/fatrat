#!/bin/bash

_dirname() { if [[ "$1" =~ / ]]; then echo "${1%/*}"; else echo .; fi; }

if [ -n "$JAVA_HOME" -a ! -d "$JAVA_HOME" ]; then
	unset JAVA_HOME
fi

if [ ! "$JAVA_HOME" ]; then
	if ! which java >/dev/null 2>&1; then
		exit 1
	fi

	JAVA_HOME="$(java -cp "$(_dirname $0)/fatrat-jplugins.jar" info.dolezel.fatrat.plugins.helpers.GetJavaHome)"
fi

libjvm=$(find -L "$JAVA_HOME" -name libjvm.so | head -n1)

if [ "x$libjvm" = "x" ]; then
	exit 2
fi

echo "$libjvm"
exit 0
