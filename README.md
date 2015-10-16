machinetalk-protobuf
====================

Protobuf declarations for machinekit messages

This repo is integrated into github.com/machinekit/machinekit as a git subtree.

To change/add to message definitions:

* send a PR against this repo
* add a new remote in your machinekit repo referring to here
* update the subtree in your machinekit repo like so

`````
git remote add machinetalk-protobuf git://github.com/machinekit/machinetalk-protobuf.git
git fetch machinetalk-protobuf
git subtree merge --prefix=src/machinetalk/proto machinetalk-protobuf/master --squash
`````

Now create a PR against the machinekit repo.
