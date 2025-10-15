CC = gcc
CFLAGS-common = -std=gnu18 -Wall -Wextra -Werror -pedantic -Iinclude
CFLAGS = $(CFLAGS-common) -O2
CFLAGS-dbg = $(CFLAGS-common) -Og -ggdb

TARGET = wsh

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
RELEASEDIR = $(BUILDDIR)/release
DEBUGDIR = $(BUILDDIR)/debug

# Source files (in src directory)
SOURCES = wsh.c dynamic_array.c hash_map.c utils.c

# Full paths to source files
SRC = $(addprefix $(SRCDIR)/,$(SOURCES))

# Object files
OBJ = $(patsubst $(SRCDIR)/%.c,$(RELEASEDIR)/%.o,$(SRC))
OBJ-dbg = $(patsubst $(SRCDIR)/%.c,$(DEBUGDIR)/%.o,$(SRC))

# Default target
all: $(TARGET) $(TARGET)-dbg

# Optimized build
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# Debug build
$(TARGET)-dbg: $(OBJ-dbg)
	$(CC) $(CFLAGS-dbg) $^ -o $@

# Compile release objects
$(RELEASEDIR)/%.o: $(SRCDIR)/%.c | $(RELEASEDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile debug objects
$(DEBUGDIR)/%.o: $(SRCDIR)/%.c | $(DEBUGDIR)
	$(CC) $(CFLAGS-dbg) -c $< -o $@

# Ensure build directories exist
$(RELEASEDIR) $(DEBUGDIR):
	mkdir -p $@

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR) $(TARGET) $(TARGET)-dbg

# Phony targets
.PHONY: all clean

# Dependencies (optional but recommended)
-include $(OBJ:.o=.d)
-include $(OBJ-dbg:.o=.d)