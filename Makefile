#	File:    Makefile
#	Date:    01.04.2023
# Project: SNT
#	Author:  Bc. Jozef Méry - xmeryj00@vut.cz

# $@ - target
# @D - target directory
# $< - first dep
# $^ - all deps
# $* - auto variable

ARCHIVE     = xmeryj00-snt-devs-2023.zip

# directory definitions
BINDIR      = bin
SRCDIR      = src
OBJDIR      = obj
INCLUDEDIR  = include

# archive content definition
ARCHIVELIST = $(SRCDIR)/ $(INCLUDEDIR)/ Makefile README.txt

# helper programs
ARCHIVER  = zip -r

# target name
TARGET    = devs_app

# file extensions
SRCEXT    = cpp
OBJEXT    = o
HDREXT    = hpp

# compiler options
CC          = g++
PLATFORM    = -m64
CFLAGS      = -pedantic -Wextra -Wall $(PLATFORM)
RELCFLAGS   = -O2 -s -DNDEBUG -flto
DCFLAGS     = -g -O0
STD         = c++17
EXTRACFLAGS = -Werror

# additional includes
INCLUDES  = $(addprefix -I,)

# linker options
LFLAGS    = $(PLATFORM)

# link libraries
LIBS    = $(addprefix -l, )
LIBDIRS = $(addprefix -L, )

default: release
all: default
.PHONY: default all release debug run run-% debug-run clean-run clean archive format

RELDIR  = Release
DDIR    = Debug

# fetch sources
SOURCES  = $(wildcard $(SRCDIR)/*.$(SRCEXT)) $(wildcard $(SRCDIR)/**/*.$(SRCEXT))
# convert to obj name
RELOBJECTS  = $(patsubst $(SRCDIR)/%.$(SRCEXT), $(OBJDIR)/$(RELDIR)/%.$(OBJEXT), $(SOURCES))
DOBJECTS  	= $(patsubst $(SRCDIR)/%.$(SRCEXT), $(OBJDIR)/$(DDIR)/%.$(OBJEXT), $(SOURCES))
# fetch headers
HEADERS  		= $(wildcard $(INCLUDEDIR)/**/*.$(HDREXT))

CREATE_DIRECTORY	= @mkdir -p $(@D)

# compile in release mode
$(OBJDIR)/$(RELDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) $(HEADERS)
	$(CREATE_DIRECTORY)
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -I./$(INCLUDEDIR) $(INCLUDES) -std=$(STD) $(RELCFLAGS) -c $< -o $@

# link release objects
$(BINDIR)/$(TARGET): $(RELOBJECTS)
	$(CREATE_DIRECTORY)
	$(CC) $^ $(LIBS) $(LIBDIRS) $(LFLAGS) -o $@

# compile in debug mode
$(OBJDIR)/$(DDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) $(HEADERS)
	$(CREATE_DIRECTORY)
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -I./$(INCLUDEDIR) $(INCLUDES) -std=$(STD) $(DCFLAGS) -c $< -o $@

# # link debug objects
$(BINDIR)/$(TARGET)_d: $(DOBJECTS)
	$(CREATE_DIRECTORY)
	$(CC) $^ $(LIBS) $(LIBDIRS) $(LFLAGS) -o $@

release: $(BINDIR)/$(TARGET)
debug: $(BINDIR)/$(TARGET)_d

# run
run: release
	@./$(BINDIR)/$(TARGET) $(ARGS)

run-%: release
	@./$(BINDIR)/$(TARGET) $*

# run debug
debug-run: debug
	@./$(BINDIR)/$(TARGET)_d $(ARGS)

# run with clear
clean-run: release
	@clear
	@./$(BINDIR)/$(TARGET) $(ARGS)

# clean directory
clean:
	-rm -rf $(OBJDIR)/
	-rm -rf $(BINDIR)/
	-rm -f  $(ARCHIVE)

# create final archive
archive:
	$(ARCHIVER) $(ARCHIVE) $(ARCHIVELIST)

format:
	@clang-format -style=file -i $(SOURCES) $(HEADERS)