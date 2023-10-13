#!/bin/sh
$REALTIME start
halcmd -f expected
halcmd save
halcmd unload all
$REALTIME stop
