#!/bin/sh
realtime start
halcmd -f expected
halcmd save
halcmd unload all
realtime stop
