BUILD = release


########## Available targets

cxxflags.debug =    -DTRACE_ON -g
linkflags.debug =   -g

cxxflags.trace =    -DNDEBUG -DTRACE_ON -O3
linkflags.trace =

cxxflags.release =  -DNDEBUG -O3
linkflags.release =

cxxflags.gprof =    -DNDEBUG -O3 -g -pg
linkflags.gprof =   -g -pg

cxxflags.stats =    -DNDEBUG -DSTATS_ON -O3
linkflags.stats =


########## Shared options

SOURCE_DIR = src
BUILD_DIR = build/$(BUILD)

GCC = g++
CXX = $(GCC) -std=c++11
CXXFLAGS = $(cxxflags.$(BUILD)) -DINCREMENTAL -Isrc/glucose-syrup -Wall -Wextra
LINK = $(GCC)
LINKFLAGS = $(linkflags.$(BUILD))
LIBS = -lm -lz -lpthread

APPS = $(shell find -L $(SOURCE_DIR) -name '*.cpp')
SRCS = $(shell find -L $(SOURCE_DIR) -name '*.cc')

BINARIES = $(patsubst $(SOURCE_DIR)%.cpp,$(BUILD_DIR)%, $(APPS))

OBJS = $(patsubst $(SOURCE_DIR)%.cc,$(BUILD_DIR)%.o, $(SRCS))
DEPS = $(patsubst $(SOURCE_DIR)%.cc,$(BUILD_DIR)%.d, $(SRCS)) $(patsubst $(SOURCE_DIR)%.cpp,$(BUILD_DIR)%.d, $(APPS))

all: $(BINARIES)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.d: $(SOURCE_DIR)/%.cc
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MM -MT '$(@:.d=.o)' $< -MF $@

$(BUILD_DIR)/%.d: $(SOURCE_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MM -MT '$(@:.d=.o)' $< -MF $@
	
$(BINARIES): % : %.o $(OBJS)
	$(LINK) $@.o $(OBJS) -o $@ $(LINKFLAGS) $(LIBS)

static: $(BINARIES)
	for bin in $(BINARIES); do \
	    $(LINK) $$bin.o $(OBJS) -static -o $$bin-static $(LINKFLAGS) $(LIBS); \
	    strip $$bin-static; \
    done

lib: $(BUILD_DIR)/zuccherino.a

$(BUILD_DIR)/zuccherino.a: $(OBJS)
	ar rcs $@ $(OBJS)

########## Tests
#include tests/Makefile.tests.inc


########## Clean
.PHONY: clean-dep clean distclean

clean-dep:
	rm -f $(DEPS) $(patsubst $(SOURCE_DIR)%.cpp,$(BUILD_DIR)%.d, $(APPS))
clean: clean-dep
	rm -f $(OBJS) $(patsubst $(SOURCE_DIR)%.cpp,$(BUILD_DIR)%.o, $(APPS))

distclean: clean
	rm -fr $(BUILD_DIR)

-include $(DEPS)
