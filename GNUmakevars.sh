: ${PREFIX:=/usr/local}
echo BINDIR=$PREFIX/bin						>> GNUmakevars.$$
echo MANDIR=$PREFIX/man						>> GNUmakevars.$$
echo MAN1DIR="\$(MANDIR)/man1"					>> GNUmakevars.$$

for i in /usr/bin/install /usr/local/bin/install /opt/imake/bin/install
do
    if [ -f $i -a -x $i ]
    then
	install=$i
	break
    fi
done
if [ -z "$install" ]
then
    echo "could not find install command, sorry" >&2
    exit 1
fi
echo INSTALL=$install						>> GNUmakevars.$$

gcc=`which gcc`
if [ -n "$gcc" ]
then
    echo "CC=gcc -MD -Wall -Wpointer-arith \\"			>> GNUmakevars.$$
    echo "    -Wstrict-prototypes -Wmissing-prototypes \\"	>> GNUmakevars.$$
    echo "    -Wnested-externs"					>> GNUmakevars.$$
    echo "CFLAGS=-O3"						>> GNUmakevars.$$
else 
    echo "CC=cc"						>> GNUmakevars.$$
    echo "CFLAGS=-O"						>> GNUmakevars.$$
fi

echo "all:"							>> GNUmakevars.$$
mv GNUmakevars.$$ GNUmakevars
