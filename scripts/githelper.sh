
#
# This is a bash shell fragment, intended to be sourced by scripts that
# want to work with git in the emc2 repo.
#
# Sets GIT_BRANCH to the passed in branch name; if none is passed in it
# attempts to detect the current branch (this will fail if the repo is in a
# detached HEAD state).
#
# Sets GIT_TAG to the most recent signed tag (this will remain unset if no
# signed tag is found).
#


function githelper() {
    if [ -z "$1" ]; then
        GIT_BRANCH=$(git branch | egrep '^\*' | cut -d ' ' -f 2)
        if [ "$GIT_BRANCH" = "(no" ]; then
            echo "'git branch' says we're not on a branch, pass one in as an argument"
            return
        fi
    else
        GIT_BRANCH="$1"
    fi

    case $GIT_BRANCH in
        master) GIT_TAG_GLOB="v2.6*";;
        v2.5_branch) GIT_TAG_GLOB="v2.5*";;
        v2.4_branch) GIT_TAG_GLOB="v2.4*";;
        *) GIT_TAG_GLOB="*";;
    esac

    for TAG in $(git tag -l "$GIT_TAG_GLOB" | sort -r); do
        if git tag -v "$TAG" > /dev/null 2> /dev/null; then
            GIT_TAG="$TAG"
            break
        fi
    done
}

