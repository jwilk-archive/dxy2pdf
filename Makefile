# Copyright © 2017 Jakub Wilk <jwilk@jwilk.net>
# SPDX-License-Identifier: MIT

CXXFLAGS = -Wall -O2 -g --std=gnu++11

.PHONY: all
all: dxy2pdf

.PHONY: clean
clean:
	rm -f dxy2pdf

# vim:ts=4 sts=4 sw=4 noet
