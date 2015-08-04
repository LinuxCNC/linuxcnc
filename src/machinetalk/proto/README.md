machinetalk-protobuf
====================

Protobuf declarations for machinekit messages

This repo is integrated into github.com/machinekit/machinekit as a git subtree.

To change message definitions, send a PR against this repo, then update the subtree in the machinekit repo like so:

`````
git remote add machinetalk-protobuf git://github.com/machinekit/machinetalk-protobuf.git
git fetch machinetalk-protobuf
git merge --squash -s subtree --no-commit  machinetalk-protobuf/master
git commit
... add appropriate commit message...
`````
