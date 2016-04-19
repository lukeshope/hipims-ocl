ifeq (,$(wildcard /usr/local/bin/mpic++))
	CPP = g++
	MACROS := -D MPI_OFF
else
	CPP = /usr/local/bin/mpic++
	MACROS := -D MPI_ON
endif

CPP_FILES := $(wildcard src/*.cpp) $(wildcard src/*/*.cpp) $(wildcard src/*/*/*.cpp) $(wildcard src/*/*/*/*.cpp) $(wildcard src/*/*/*/*/*.cpp)
OBJ_FILES := $(patsubst %.cpp,%.o,$(CPP_FILES))
LD_FLAGS := -L/opt/AMDAPP/lib/x86_64/ -L/usr/local/browndeer/lib/
LD_LINKS := -rdynamic -lm -lboost_system -lboost_regex -lboost_filesystem -lOpenCL -lgdal -lncurses -lpthread
CC_FLAGS := -rdynamic -g -Wall -g3 -w -I/usr/local/cuda-7.0/include/ -I/usr/local/include/ -I/usr/include/gdal/ -I/opt/AMDAPP/include/ -I/usr/local/browndeer/include/ -std=c++0x $(MACROS)

hipims: $(OBJ_FILES)
	$(CPP) $(LD_FLAGS) -o bin/linux64/$@ $^ $(LD_LINKS)

%.o: %.cpp
	$(CPP) $(CC_FLAGS) -c -o $@ $<

clean:
	find . -name \*.o -execdir rm {} \;
	rm -rf bin/linux64/*

release:
	find . -name \*.cpp -execdir rm {} \;
	find . -name \*.o -execdir rm {} \;
	find . -name \*.h -execdir rm {} \;
	find . -name \*.log -execdir rm {} \;
	find . -name \*.vcx* -execdir rm {} \;
	find . -name \*.rc -execdir rm {} \;
	find . -name \*.aps -execdir rm {} \;

