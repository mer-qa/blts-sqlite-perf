#!/bin/sh -e

aclocal
autoheader
autoconf
automake --add-missing --copy
