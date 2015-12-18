#!/bin/bash -ex

branch="${TAG}/${CMD}"
if [ ! -z ${FLAV+x} ]; then
    branch+="/${FLAV}"
fi

res=$(git ls-remote git://github.com/${CCACHE_REPO})

# fetch if ccache is available
if [[ "${res}" = *"${branch}"* ]]; then
    git clone -b ${branch} --depth=1 git://github.com/${CCACHE_REPO} ../ccache
    # delete git directory, no need for history
    rm -rf ../ccache/.git
else # empty ccache
    mkdir ../ccache
fi
