# Makefile for acct_gather_energy/none plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = acct_gather_infiniband_none.la

# Null job completion logging plugin.
acct_gather_infiniband_none_la_SOURCES = acct_gather_infiniband_none.c

acct_gather_infiniband_none_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)

