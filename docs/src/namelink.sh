echo namelink.sh------------------------------------------
# make links for files using png files made by latexmath
script=$0
file=$1
newname=$2

dir=$(dirname $file)
echo "$script:    file=$file"
echo "$script:     dir=$dir"
echo "$script: newname=$newname"

cd ${dir}
for f in *.png ;do
  suffix=${f##*__}
  linkname=${newname}__${suffix}
  if [ -h $linkname ] ; then continue ; fi
  ln -s $f $linkname
  echo "$f <-- $linkname"
done
echo namelink.sh------------------------------------------
exit 0
