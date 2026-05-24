#!/bin/sh

set -eu #Needed so CI fails when anything is wrong

#To let the script fail if git status has any issues
#If a command in $() fails, the script just continues
git status -u --porcelain -- "$@" > /dev/null

if [ $# -gt 0 ]; then
    #Arguments are pathspecs for git status, used to exclude files for this test
    if [ -n "$(git status -u --porcelain -- "$@")" ]; then
        echo "Build produced untracked or modified files:----------------------------------------"
        git status -u --porcelain -- "$@"
        echo "-----------------------------------------------------------------------------------"
        echo "Pathspec is: \"$*\", withouth pathspec:--------------------------------------------"
        git status -u --porcelain
        echo "-----------------------------------------------------------------------------------"
        exit 1
    else
        echo Repo is clean
        echo "Pathspec is: \"$*\", withouth pathspec:--------------------------------------------"
        git status -u --porcelain
        echo "-----------------------------------------------------------------------------------"
        exit 0
    fi
else
    #No arguments: Test all files
    if [ -n "$(git status -u --porcelain)" ]; then
        echo "Build produced untracked or modified files:----------------------------------------"
        git status -u --porcelain
        echo "-----------------------------------------------------------------------------------"
        exit 1
    else
        echo Repo is clean
        exit 0
    fi
fi
