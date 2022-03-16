SCROOT = $(HOME)/src/SingularComputingMaterialProvidedToLANL/System\ Code
CPPFLAGS = -I$(SCROOT)
CXXFLAGS = -g -O2 -Wno-write-strings
LDFLAGS = -L$(SCROOT)
LIBS = -lS1

SOURCES = main.cpp emit.cpp
OBJECTS = $(patsubst %.cpp,%.o,$(SOURCES))

all: simple-bcmc

simple-bcmc: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o simple-bcmc $(OBJECTS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

clean:
	$(RM) simple-bcmc $(OBJECTS)

.PHONY: all clean
