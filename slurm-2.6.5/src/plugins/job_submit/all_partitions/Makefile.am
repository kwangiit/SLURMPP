# Makefile for job_submit/all_partitions plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = job_submit_all_partitions.la

# Job submit all_partitions plugin.
job_submit_all_partitions_la_SOURCES = job_submit_all_partitions.c
job_submit_all_partitions_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)
