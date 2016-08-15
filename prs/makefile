# Portable Runtime System (PRS)
# Copyright (C) 2016  Alexandre Tremblay
# 
# This file is part of PRS.
# 
# PRS is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# portableruntimesystem@gmail.com

# Include the top makefile which will define the build characteristics
MAKEFILE.TOP := $(CURDIR)/../make/makefile.top
include $(MAKEFILE.TOP)

# Define the target information
TARGET = prs

# Define the generic source files to build
SOURCES += alloc/stack.c
SOURCES += lib/ds/dllist.c
SOURCES += lib/ds/idllist.c
SOURCES += lib/ds/mpmcq.c
SOURCES += lib/ds/mpmcring.c
SOURCES += lib/ds/mpscq.c
SOURCES += lib/ds/mpsciq.c
SOURCES += lib/ds/pd.c
SOURCES += lib/ds/pool.c
SOURCES += lib/str.c
SOURCES += assert.c
SOURCES += clock.c
SOURCES += error.c
SOURCES += event.c
SOURCES += excp.c
SOURCES += god.c
SOURCES += gpd.c
SOURCES += init.c
SOURCES += log.c
SOURCES += main.c
SOURCES += msgq.c
SOURCES += name.c
SOURCES += pr.c
SOURCES += proc.c
SOURCES += rtc.c
SOURCES += sched.c
SOURCES += sched/swcoop.c
SOURCES += sched/swprio.c
SOURCES += spinlock.c
SOURCES += svc/log.c
SOURCES += svc/proc.c
SOURCES += systeminfo.c
SOURCES += sem.c
SOURCES += task.c
SOURCES += worker.c
SOURCES += timer.c

# Define test-specific source files to build
ifdef TEST_DB_PATH
DEFINES += PRS_TEST
SOURCES += lib/sqlite/sqlite3.c
SOURCES += lib/test/test.c
SOURCES += svc/test.c
INCLUDEDIRS += lib/sqlite
endif

# Define the specific source files to build
SOURCES += pal/stdc/exit.c
SOURCES += pal/stdc/malloc.c
ifeq ($(OS),windows)
	SOURCES += pal/windows/assert.c
	SOURCES += pal/windows/context.c
	SOURCES += pal/windows/excp.c
	SOURCES += pal/windows/mem.c
	SOURCES += pal/windows/os.c
	SOURCES += pal/windows/pit.c
	SOURCES += pal/windows/proc.c
	SOURCES += pal/windows/thread.c
	SOURCES += pal/windows/wls.c
	
	DEFINES += PSAPI_VERSION=1
	LIBS += psapi
	LIBS += winmm
	LIBS += version
else
ifeq ($(OS),linux)
	SOURCES += pal/linux/os.c
	SOURCES += pal/linux/proc.c
	SOURCES += pal/posix/assert.c
	SOURCES += pal/posix/context.c
	SOURCES += pal/posix/excp.c
	SOURCES += pal/posix/mem.c
	SOURCES += pal/posix/pit.c
	SOURCES += pal/posix/signal.c
	SOURCES += pal/posix/thread.c
	SOURCES += pal/stdc/wls.c

	LIBS += pthread
	LIBS += dl
endif
endif

# Define the include paths
INCLUDEDIRS += \
	../include

# Use the latest C version
ifdef C99
	CCFLAGS += --std=c99
else
	CCFLAGS += --std=c11
endif

# Enable all warnings
CFLAGS += -Wall

ifeq ($(OS),windows)
	# Link libgcc statically so we do not depend on external DLLs
	LDFLAGS += -static-libgcc

	# Export symbols
	LDFLAGS += -Wl,--enable-auto-import -Wl,--exclude-all-symbols

	# Flags required when using the Very Sleepy profiler
	CFLAGS += -fno-omit-frame-pointer -gdwarf-2
endif

ifeq ($(OS), linux)
	# Build with _GNU_SOURCE for non-POSIX features
	CFLAGS += -D_GNU_SOURCE
	
	# Use position-independent code so that the executable can also be used as
	# a shared library
	CFLAGS += -fPIC
	LDFLAGS += -pie -Wl,-E
endif

# Test database path
ifdef TEST_DB_PATH
	DEFINES += PRS_TEST_DB_PATH=\"$(TEST_DB_PATH)\"
endif

# Include rules
include $(MAKEDIR)/makefile.rules

# Shared library linking
SLTARGET := $(BINDIR)/$(SL_PRE)$(TARGET)$(SL_EXT)
ifeq ($(OS), windows)
# On Windows, we need to link a stub DLL library so that external applications can link to the PRS APIs
SLFLAGS += $(LDFLAGS) -shared
$(SLTARGET): $(OBJS)
	$(ECHO) Linking $@...
	$(call ld,$(SLFLAGS) $^,$@)
else
# On Linux, we only need to copy the existing executable as it was already lin
$(SLTARGET): $(BINDIR)/$(TARGET)$(EXE_EXT)
	$(ECHO) Copying $@...
	$(V)$(CP) $(call native_path,$(BINDIR)/$(TARGET)$(EXE_EXT)) $(call native_path,$(SLTARGET))
endif

all: $(SLTARGET)
