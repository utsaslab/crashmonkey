CODEDIR = code
SUBDIRS = $(CODEDIR)
SUBDIRS_CLEAN = $(addsuffix .clean, $(SUBDIRS))
BUILD_DIR = build

.PHONY: all clean $(SUBDIRS) $(SUBDIRS_CLEAN)

all: $(SUBDIRS)

tests:
	$(MAKE) -C $(CODEDIR) tests

permuters:
	$(MAKE) -C $(CODEDIR) permuters

$(SUBDIRS):
	$(MAKE) -C $@ all

$(SUBDIRS_CLEAN):
	$(MAKE) -C $(subst .clean, , $@) clean

clean: $(SUBDIRS_CLEAN)
	rm -rf build
