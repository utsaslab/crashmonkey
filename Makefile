CODEDIR = code
FSSTRESSDIR = code/fsstress
SUBDIRS = $(CODEDIR)
SUBDIRS_CLEAN = $(addsuffix .clean, $(SUBDIRS))
FSSTRESSDIR_CLEAN = $(addsuffix .clean, $(FSSTRESSDIR))
BUILD_DIR = build

.PHONY: all clean $(SUBDIRS) $(SUBDIRS_CLEAN) $(FSSTRESSDIR) $(FSSTRESSDIR_CLEAN)

all: $(SUBDIRS) $(FSSTRESSDIR)

tests:
	$(MAKE) -C $(CODEDIR) tests

permuters:
	$(MAKE) -C $(CODEDIR) permuters

$(SUBDIRS):
	$(MAKE) -C $@ all

$(FSSTRESSDIR):
	$(MAKE) -C $@ 

$(SUBDIRS_CLEAN):
	$(MAKE) -C $(subst .clean, , $@) clean

$(FSSTRESSDIR_CLEAN):
	$(MAKE) -C $(subst .clean, , $@) clean

clean: $(SUBDIRS_CLEAN) $(FSSTRESSDIR_CLEAN)
	rm -rf build \
	rm code/fsstress/fsstress
