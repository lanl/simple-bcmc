SCROOT = $(HOME)/src/SingularComputingMaterialProvidedToLANL/System\ Code
CPPFLAGS = -I$(SCROOT) -I.
CXXFLAGS = -g -O2 -Wno-write-strings -std=c++17
LDFLAGS = -L$(SCROOT)
LIBS = -lS1

SOURCES = \
	main.cpp \
	imc.cpp \
	threefry.cpp \
	utils.cpp
OBJECTS = $(patsubst %.cpp,%.o,$(SOURCES))

all: simple-bcmc

simple-bcmc: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o simple-bcmc $(OBJECTS) $(LDFLAGS) $(LIBS)

%.o: %.cpp novapp.h simple-bcmc.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

clean:
	$(RM) simple-bcmc $(OBJECTS)

.PHONY: all clean
