# Copyright © 2017 Jakub Wilk <jwilk@jwilk.net>
# SPDX-License-Identifier: MIT

CXXFLAGS = -Wall -O2 -g --std=gnu++11

ifneq "$(findstring -mingw,$(CXX))" ""
LDLIBS += -lcomdlg32
endif

.PHONY: all
all: dxy2pdf

.PHONY: clean
clean:
	rm -f dxy2pdf

.error = GNU make is required

# vim:ts=4 sts=4 sw=4 noet
