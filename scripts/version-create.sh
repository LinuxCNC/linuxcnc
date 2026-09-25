#!/bin/bash

SRC_BASE_VERSION=$(cat VERSION_BASE)

#Check if this is a git repo
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Not a Git repository"
    VERSION=$SRC_BASE_VERSION-0-unknown
    echo $VERSION
    echo $VERSION > VERSION
    exit 0
fi

#Check if current commit is tagged and get tag name
GIT_VERSION=$(git describe --tags --exact-match 2>/dev/null)
#Remove leading v
GIT_VERSION=$(echo $GIT_VERSION | sed -re 's/^v(.*)$/\1/')

if [ -n "$GIT_VERSION" ]; then
    #Tagged, check if VERSION_BASE matches tag
    
    echo "Git tag version: \"$GIT_VERSION\" Src Version: \"$SRC_BASE_VERSION\""
    if [ "$GIT_VERSION" = "$SRC_BASE_VERSION" ]; then
        echo Tagged and version match, using VERSION_BASE as final version
        cp VERSION_BASE VERSION
        exit 0
    else
        echo ERROR: Tagged but tag and VERSION_BASE do not match, correct either tag or VERSION_BASE
        exit 1
    fi
else
    #Not tagged, development build
    #Try to get correct version based on tag
    GIT_VERSION_TAG="v$SRC_BASE_VERSION"
    GIT_DEV_VERSION=$(git describe --match "$GIT_VERSION_TAG" 2>/dev/null)
    if [ -n "$GIT_DEV_VERSION" ]; then
        echo Version based on tag $GIT_VERSION_TAG: $GIT_DEV_VERSION
        echo $GIT_DEV_VERSION > VERSION
    else
        echo No tag matching $GIT_VERSION_TAG, version counter based on last change in VERSION_BASE
        COUNT=$(git rev-list --count $(git log -1 --format='%H' VERSION_BASE)..HEAD)
        HASH=$(git log --pretty=format:'%h' -n 1)
        VERSION=$SRC_BASE_VERSION-$COUNT-g$HASH
        echo $VERSION
        echo $VERSION > VERSION
    fi
fi
