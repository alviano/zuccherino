#!/bin/sh

DIRNAME=`dirname $0`
OLDPATH=`pwd`

echo "Removing directory src/glucose-syrup (if it exists)"
rm -rf $DIRNAME/src/glucose-syrup

echo "Downloading glucose 4.1..."
if [ ! -e $DIRNAME/patches/glucose-syrup.tgz ]; then
    wget -O $DIRNAME/patches/glucose-syrup.tgz http://www.labri.fr/perso/lsimon/downloads/softwares/glucose-syrup-4.1.tgz
fi

echo "Patching glucose 4.1..."
cd $DIRNAME/patches; rm -rf glucose-syrup; tar xf glucose-syrup.tgz; mv glucose-syrup-4.1 glucose-syrup; cd $OLDPATH
cd $DIRNAME/patches/glucose-syrup; patch -p1 <../glucose-syrup.4.1.patch; cd $OLDPATH
mv $DIRNAME/patches/glucose-syrup $DIRNAME/src/

echo
echo "You can now run make!"
