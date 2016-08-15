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
MAKEFILE.TOP := $(CURDIR)/make/makefile.top
include $(MAKEFILE.TOP)

TARGETS = prs
ifneq ($(wildcard test/*),)
	TARGETS += test
endif
ifneq ($(wildcard examples/*),)
	TARGETS += examples
endif

# Define targets for 'all'
ALL_TARGETS := $(addsuffix .all,$(TARGETS))

# Define targets for 'clean'
CLEAN_TARGETS := $(addsuffix .clean,$(TARGETS))

# Define phony targets that are not file-related
.PHONY: $(ALL_TARGETS) $(CLEAN_TARGETS) test

# Redirect targets to sub-directories
all: $(ALL_TARGETS)
clean: $(CLEAN_TARGETS)
test:
ifneq ($(wildcard test/*),)
	$(V)$(MAKE) -C test test
endif
ifneq ($(wildcard examples/*),)
	$(V)$(MAKE) -C examples test
endif

$(ALL_TARGETS):
	$(V)$(MAKE) -C $(basename $@) all

$(CLEAN_TARGETS):
	$(V)$(MAKE) -C $(basename $@) clean
