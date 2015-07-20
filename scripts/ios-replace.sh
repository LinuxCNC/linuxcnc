#!/bin/sh
# Replace "namespace google" with "namespace google_public"
# in all source/header files. This is to address a
# namespace collision issue when building for recent
# versions of iOS. Apple is using the protobuf library
# internally, and embeds it as a private framework.
###############################################################

(
echo "------------------ Fixup namespace --------------------"
cd $1
sed -i '' -e 's/namespace\ google /namespace\ google_public /g' $(find . -name \*.h -type f)
sed -i '' -e 's/namespace\ google /namespace\ google_public /g' $(find . -name \*.cc -type f)
sed -i '' -e 's/namespace\ google /namespace\ google_public /g' $(find . -name \*.proto -type f)
sed -i '' -e 's/google::protobuf/google_public::protobuf/g' $(find . -name \*.h -type f)
sed -i '' -e 's/google::protobuf/google_public::protobuf/g' $(find . -name \*.cc -type f)
sed -i '' -e 's/google::protobuf/google_public::protobuf/g' $(find . -name \*.proto -type f)
)

exit 0
