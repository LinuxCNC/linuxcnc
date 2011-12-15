
#
# This is a bash shell fragment, intended to be sourced by scripts that
# want to work with git in the emc2 repo.
#
# Sets GIT_BRANCH to the passed in branch name; if none is passed in it
# attempts to detect the current branch (this will fail if the repo is in a
# detached HEAD state).
#
# Sets DEB_COMPONENT based on the branch.  Official release branches get
# their own component, all other branches go in "scratch".
#
# Sets GIT_TAG to the most recent signed tag (this will fall back to the
# most recent tag of any kind if no signed tag is found).
#


function githelper() {
    if [ -z "$1" ]; then
        GIT_BRANCH=$(git branch | egrep '^\*' | cut -d ' ' -f 2)
        if [ "$GIT_BRANCH" = "(no" ]; then
            echo "'git branch' says we're not on a branch, pass one in as an argument" > /dev/null 1>&2
            return
        fi
    else
        GIT_BRANCH="$1"
    fi

    case $GIT_BRANCH in
        master)
            GIT_TAG_GLOB="v2.6*"
            DEB_COMPONENT="master"
            ;;
        v2.5_branch)
            GIT_TAG_GLOB="v2.5*"
            DEB_COMPONENT="v2.5_branch"
            ;;
        v2.4_branch)
            GIT_TAG_GLOB="v2.4*"
            DEB_COMPONENT="v2.4_branch"
            ;;
        *)
            GIT_TAG_GLOB="*"
            DEB_COMPONENT="scratch"
            ;;
    esac

    for TAG in $(git tag -l "$GIT_TAG_GLOB" | sort -r); do
        if git tag -v "$TAG" > /dev/null 2> /dev/null; then
            GIT_TAG="$TAG"
            break
        fi
    done

    if [ -z "$GIT_TAG" ]; then
        GIT_TAG=$(git tag -l "$GIT_TAG_GLOB" | sort -r | head -1)
        echo "could not verify tag signatures, using $GIT_TAG" > /dev/null 1>&2
    fi
}

