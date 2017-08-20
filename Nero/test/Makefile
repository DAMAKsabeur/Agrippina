CC = /home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/host/usr/bin/i686-cm-linux-g++
CXX = /home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/host/usr/bin/i686-cm-linux-g++
CPPFLAGS = -I/home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/staging/include
CPPFLAGS +=-I/home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/staging/include/linux_user
CPPFLAGS +=-I/home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/build/Nero_smd/include
CPPFLAGS +=-I/home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/build/Nero_smd/NeroIntelCE4x00/include/
LDFLAGS= -L/home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/staging/lib
CFLAGS = -Wall
EXEC_NAME = NeroTsReader
INCLUDES = 

LIBS = -lgdl -lismd_demux -lismd_audio -lismd_viddec -lismd_vidpproc -lismd_vidrend -lsven -losal -lpal -lplatform_config -lcommon_debug -lismd_core -lismd_vidsink_direct
OBJ_FILES =../NeroIntelCE4x00/src/NeroIntelCE4x00STC.o ../NeroIntelCE4x00/src/NeroIntelCE4x00AudioDecoder.o ../NeroIntelCE4x00/src/NeroIntelCE4x00VideoDecoder.o  ../src/NeroAudioDecoder.o ../src/NeroSTC.o ../src/NeroVideoDecoder.o  NeroDemux.o Nero_main.o
INSTALL_DIR = /home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/target/bin
INSTALL_NFS_DIR = /home/damak/Documents/
all : $(EXEC_NAME)

clean :
	rm $(EXEC_NAME) $(OBJ_FILES)

$(EXEC_NAME) : $(OBJ_FILES)
	$(CXX) -o $(EXEC_NAME) $(OBJ_FILES) $(LDFLAGS) $(LIBS) $(CPPFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LIBS) $(CPPFLAGS) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LIBS) $(CPPFLAGS) -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LIBS) $(CPPFLAGS) -o $@ -c $<

install :
	cp $(EXEC_NAME) $(INSTALL_DIR)
	sudo cp $(EXEC_NAME) $(INSTALL_NFS_DIR)
