MSRC = miracl/source
MINC = miracl/include
MEXTRA = miracl_extra/

CLMULFLAGS =
CLMULOBJ = 
CLMULDEF = 

ifeq ($(shell uname), Darwin)
	CCMP = /opt/local/bin/g++-mp-4.9
	ifneq ($(wildcard $(CCMP)),)
		CC = $(CCMP)
	else
		CC = g++-4.9
	endif

	ifneq (,$(findstring PCLMULQDQ,$(shell sysctl -a)))
		CLMULFLAGS = -msse2 -msse4.1 -mpclmul -Wa,-q
		CLMULOBJ = $(MSRC)/mrcomba2.o
		CLMULDEF = \#define MR_COMBA2 $(KCMCOMBASTEP)
	endif
else
	CC = g++
	ifneq (,$(findstring pclmulqdq,$(shell cat /proc/cpuinfo)))
		CLMULFLAGS = -msse2 -msse4.1 -mpclmul -Wa,-q
		CLMULOBJ = $(MSRC)/mrcomba2.o
		CLMULDEF = \#define MR_COMBA2 $(KCMCOMBASTEP)
	endif
endif

ifeq ($(shell getconf LONG_BIT), 64)
	MRMULDV = $(MSRC)/mrmuldv.g64
	MCS = $(MSRC)/amd64
	KCMCOMBASTEP = 4
else
	MRMULDV = $(MSRC)/mrmuldv.gcc
	MCS = $(MSRC)/gccsse2
	KCMCOMBASTEP = 8
endif

OBJ = ledger.o zlutil.o lepprocessor.o lbpprocessor.o dbpprocessor.o
MOBJ = $(MSRC)/mrcore.o $(MSRC)/mrarth0.o $(MSRC)/mrarth1.o $(MSRC)/mrarth2.o $(MSRC)/mralloc.o $(MSRC)/mrsmall.o $(MSRC)/mrio1.o $(MSRC)/mrio2.o $(MSRC)/mrgcd.o $(MSRC)/mrjack.o $(MSRC)/mrxgcd.o $(MSRC)/mrarth3.o $(MSRC)/mrbits.o $(MSRC)/mrrand.o $(MSRC)/mrprime.o $(MSRC)/mrcrt.o $(MSRC)/mrscrt.o $(MSRC)/mrmonty.o $(MSRC)/mrpower.o $(MSRC)/mrsroot.o $(MSRC)/mrcurve.o $(MSRC)/mrfast.o $(MSRC)/mrshs.o $(MSRC)/mrshs256.o $(MSRC)/mrshs512.o $(MSRC)/mrsha3.o $(MSRC)/mrfpe.o $(MSRC)/mraes.o $(MSRC)/mrgcm.o $(MSRC)/mrlucas.o $(MSRC)/mrzzn2.o $(MSRC)/mrzzn2b.o $(MSRC)/mrzzn3.o $(MSRC)/mrecn2.o $(MSRC)/mrstrong.o $(MSRC)/mrbrick.o $(MSRC)/mrebrick.o $(MSRC)/mrec2m.o $(MSRC)/mrgf2m.o $(MSRC)/mrflash.o $(MSRC)/mrfrnd.o $(MSRC)/mrdouble.o $(MSRC)/mrround.o $(MSRC)/mrbuild.o $(MSRC)/mrflsh1.o $(MSRC)/mrpi.o $(MSRC)/mrflsh2.o $(MSRC)/mrflsh3.o $(MSRC)/mrflsh4.o $(MSRC)/mrmuldv.o $(MSRC)/big.o $(MSRC)/zzn.o $(MSRC)/ecn.o $(MSRC)/ec2.o $(MSRC)/flash.o $(MSRC)/crt.o $(MSRC)/mrkcm.o $(MSRC)/mrcomba.o $(CLMULOBJ)
DEPS = $(MINC)/mirdef.h
CFLAGS = -I$(MINC) -march=native -pthread -O2 -std=c++11 $(CLMULFLAGS)
LDFLAGS =
LDLIBS = miracl/miracl.a

default: zlgenerate zlverify

%.o: %.c $(DEPS) 
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(DEPS) 
	$(CC) $(CFLAGS) -c -o $@ $<

miracl/mex:
	$(CC) $(CFLAGS) -o miracl/mex miracl/mex.c

$(MSRC)/mrkcm.o: miracl/mex
	miracl/mex 4 $(MCS) $(MSRC)/mrkcm
	$(CC) $(CFLAGS) -c -o $@ $(MSRC)/mrkcm.c

$(MSRC)/mrcomba.o: miracl/mex $(DEPS)
	miracl/mex $(KCMCOMBASTEP) $(MCS) $(MSRC)/mrcomba
	$(CC) $(CFLAGS) -c -o $@ $(MSRC)/mrcomba.c

$(MSRC)/mrcomba2.o: miracl/mex $(DEPS)
	patch -Np0 < miracl_extra/mrcomba2.patch
	miracl/mex $(KCMCOMBASTEP) $(MSRC)/clmul $(MSRC)/mrcomba2
	$(CC) $(CFLAGS) -c -o $@ $(MSRC)/mrcomba2.c

$(MSRC)/mrmuldv.o: 
	cp $(MRMULDV) $(MSRC)/mrmuldv.c
	$(CC) $(CFLAGS) -c -o $@ $(MSRC)/mrmuldv.c

$(MINC)/mirdef.h: 
	cp $(MEXTRA)/mirdef.hpp_mt $@
	echo "#define MR_COMBA $(KCMCOMBASTEP)" >> $@
	echo "$(CLMULDEF)" >> $@

miracl/miracl.a: $(MOBJ)
	ar rc $@ $(MOBJ)

zlgenerate: zlgenerate.o $(OBJ) miracl/miracl.a
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

zlverify: zlverify.o $(OBJ) miracl/miracl.a
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

zlincrementalio: zlincrementalio.o $(OBJ) miracl/miracl.a
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o zlgenerate zlverify zlincrementalio $(MSRC)/*.o $(MSRC)/mrmuldv.c $(MSRC)/mrkcm.c $(MSRC)/mrcomba.c $(MSRC)/mrcomba2.c $(MINC)/mirdef.h miracl/miracl.a miracl/mex
	patch -RNp0 < miracl_extra/mrcomba2.patch

