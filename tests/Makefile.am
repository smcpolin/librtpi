# SPDX-License-Identifier: LGPL-2.1-only
# Copyright © 2018 VMware, Inc. All Rights Reserved.

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = test cvhang cvperf
test_SOURCES = test.c $(top_srcdir)/rtpi.h
test_LDADD = -L $(top_builddir) -lrtpi -lpthread
test_CPPFLAGS= -I $(top_srcdir)

cvhang_SOURCES = cvhang.c $(top_srcdir)/rtpi.h
cvhang_LDADD = -L $(top_builddir) -lrtpi -lpthread -ldl
cvhang_CPPFLAGS= -I $(top_srcdir)

cvperf_SOURCES = cvperf.c $(top_srcdir)/rtpi.h
cvperf_LDADD = -L $(top_builddir) -lrtpi -lpthread -lm
cvperf_CPPFLAGS= -I $(top_srcdir)
