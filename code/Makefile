KERNEL_VERSION := $(shell uname -r)

GCC = gcc
GPP = g++
DEBUG_FLAGS += -g -DDEBUG
GOPTS = -std=c++11
GOTPSSO = -shared -fPIC

BUILD_DIR = $(CURDIR)/../build
TESTING := testing/log_on_off testing/test_get_log_ent_size

MODULES = cow_brd disk_wrapper
obj-m := $(addsuffix .o, $(MODULES))
KDIR := /lib/modules/$(KERNEL_VERSION)/build

# Some Makefile magic to get the right include for xattr depending on kernel
# version. To avoid including kernel headers in user test cases, we don't want
# to compare against the KERNEL_VERSION macro. Thanks to some random SO post for
# how to compare the kernel version.
KERNEL_MAJ := $(shell echo $(KERNEL_VERSION) | cut -f1 -d.)
KERNEL_MIN := $(shell echo $(KERNEL_VERSION) | cut -f2 -d.)
XATTR_DEF_FLAG :=
ifeq ($(shell [ $(KERNEL_MAJ) -gt 4 -o \
	\( $(KERNEL_MAJ) -eq 4 -a $(KERNEL_MIN) -ge 15 \) ] && echo true), true)
XATTR_DEF_FLAG := -DNEW_XATTR_INC
endif

# Names of tests containing calls to fallocate with the zero range flag.
FALLOC_ZERO_RANGE_TESTS = \
	bug2_f2fs_fzero_fdatasync.cpp \
	bug1_btrfs_falloc_fsync.cpp \
	generic_042_fzero.cpp \
	generic_042_fzero_keep_size.cpp \
	generic_042_fzero_unaligned.cpp \
	generic_042_fzero_keep_size_unaligned.cpp

# Exclude the fallocate zero range tests on kernels < 3.15 since the call
# doesn't exist in those kernels.
FALLOC_EXCLUDE :=
ifeq ($(shell [ $(KERNEL_MAJ) -lt 3 -o \
	\( $(KERNEL_MAJ) -eq 3 -a $(KERNEL_MIN) -lt 15 \) ] && echo true), true)
FALLOC_EXCLUDE := $(FALLOC_ZERO_RANGE_TESTS)
endif

CM_GEN_042_EXCLUDE = generic_042_base.cpp
CM_GEN_042 = \
		$(patsubst %.cpp, %.so, \
			$(filter-out $(CM_GEN_042_EXCLUDE), \
				$(filter-out $(FALLOC_EXCLUDE), \
					$(notdir $(wildcard $(CURDIR)/tests/generic_042/*.cpp)))))
CM_GEN_042_OUT = \
		$(foreach TEST, $(CM_GEN_042), \
			$(BUILD_DIR)/tests/generic_042/$(TEST))

CM_TESTS_EXCLUDE = BaseTestCase.cpp
CM_TESTS = \
		$(patsubst %.cpp, %.so, \
			$(filter-out $(CM_TESTS_EXCLUDE), \
				$(filter-out $(FALLOC_EXCLUDE), \
					$(notdir $(wildcard $(CURDIR)/tests/*.cpp)))))

CM_PERMUTER_EXCLUDE = Permuter.cpp
CM_PERMUTERS = \
		$(patsubst %.cpp, %.so, \
			$(filter-out $(CM_PERMUTER_EXCLUDE), \
				$(notdir $(wildcard $(CURDIR)/permuter/*.cpp))))

.PHONY: all modules c_harness user_tool $(CM_TESTS) $(CM_PERMUTERS) clean

################################################################################
# Rules used as shorthand to build things.
################################################################################

all: \
		modules \
		c_harness \
		user_tools \
		tests \
		permuters

modules: \
		$(foreach MOD, $(MODULES), $(addsuffix .c, $(MOD))) \
		$(BUILD_DIR)/Makefile
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) src=$(CURDIR) modules

c_harness: \
		$(BUILD_DIR)/c_harness

user_tools: \
		$(BUILD_DIR)/user_tools/begin_log \
		$(BUILD_DIR)/user_tools/end_log \
		$(BUILD_DIR)/user_tools/begin_tests \
		$(BUILD_DIR)/user_tools/cm_checkpoint

tests: \
		$(foreach TEST, $(CM_TESTS), $(BUILD_DIR)/tests/$(TEST)) \
		$(CM_GEN_042_OUT)

permuters: \
		$(foreach PERMUTER, $(CM_PERMUTERS), $(BUILD_DIR)/permuter/$(PERMUTER))


################################################################################
# Rules that actually build things.
################################################################################

# Rule to make Makefile in build dir so that kbuild will build our modules into
# a new directory.
$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	touch "$@"

$(BUILD_DIR)/harness/%.o: \
		harness/%.cpp
	mkdir -p $(@D)
	$(GPP) -std=c++11 $^ -c -o $@

$(BUILD_DIR)/c_harness: \
		harness/c_harness.cpp \
		harness/Tester.cpp \
		$(BUILD_DIR)/harness/FsSpecific.o \
		$(BUILD_DIR)/utils/utils.o \
		$(BUILD_DIR)/utils/communication/ServerSocket.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o \
		$(BUILD_DIR)/permuter/Permuter.o \
		$(BUILD_DIR)/results/TestSuiteResult.o \
		$(BUILD_DIR)/results/SingleTestInfo.o \
		$(BUILD_DIR)/results/FileSystemTestResult.o \
		$(BUILD_DIR)/results/DataTestResult.o \
		$(BUILD_DIR)/results/PermuteTestResult.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $^ -ldl -o $@

$(BUILD_DIR)/tests/generic_042/%.o: %.cpp
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -fPIC -c -o $@ $<

# For static pattern rules like the one below, the pattern and prereqs must be
# separated by ' :' including the space!
$(CM_GEN_042_OUT): $(BUILD_DIR)/tests/generic_042/%.so : \
		tests/generic_042/%.cpp \
		$(BUILD_DIR)/tests/BaseTestCase.o \
		$(BUILD_DIR)/tests/generic_042/generic_042_base.o \
		$(BUILD_DIR)/user_tools/src/actions.o \
		$(BUILD_DIR)/user_tools/src/workload.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o \
		$(BUILD_DIR)/utils/communication/ClientSocket.o \
		$(BUILD_DIR)/utils/communication/ClientCommandSender.o \
		$(BUILD_DIR)/results/DataTestResult.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $(GOTPSSO) $(XATTR_DEF_FLAG) -Wl,-soname,$(notdir $@) \
		-o $@ $^

$(BUILD_DIR)/tests/%.so: \
		tests/%.cpp \
		$(BUILD_DIR)/tests/BaseTestCase.o \
		$(BUILD_DIR)/user_tools/src/actions.o \
		$(BUILD_DIR)/user_tools/src/workload.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o \
		$(BUILD_DIR)/utils/communication/ClientSocket.o \
		$(BUILD_DIR)/utils/communication/ClientCommandSender.o \
		$(BUILD_DIR)/results/DataTestResult.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $(GOTPSSO) $(XATTR_DEF_FLAG) -Wl,-soname,$(notdir $@) \
		-o $@ $^

$(BUILD_DIR)/tests/%.o: \
		tests/%.cpp
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -fPIC -c -o $@ $<

$(BUILD_DIR)/results/%.o: \
		results/%.cpp
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -fPIC -c -o $@ $<

$(BUILD_DIR)/permuter/Permuter.o: \
		permuter/Permuter.cpp
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -fPIC -c -o $@ $<

$(BUILD_DIR)/permuter/%.so: \
		permuter/%.cpp \
		$(BUILD_DIR)/permuter/Permuter.o \
		$(BUILD_DIR)/utils/utils.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,RandomPermuter.so \
		-o $@ $^

$(BUILD_DIR)/utils/utils.o: \
		utils/utils.cpp
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -fPIC -c -o $@ $<

$(BUILD_DIR)/user_tools/%: \
		user_tools/%.cpp \
		$(BUILD_DIR)/user_tools/src/actions.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o \
		$(BUILD_DIR)/utils/communication/ClientSocket.o \
		$(BUILD_DIR)/utils/communication/ClientCommandSender.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -o $@ $^

$(BUILD_DIR)/user_tools/src/%.o: \
		user_tools/src/%.cpp
	mkdir -p $(@D)
	$(GPP) -c $(GOPTS) -fPIC -o $@ $<

$(BUILD_DIR)/utils/communication/%.o: \
		utils/communication/%.cpp
	mkdir -p $(@D)
	$(GPP) -c $(GOPTS) -fPIC -o $@ $<

# kbuild complains if the Makefile isn't in the build dir too.
clean: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) clean
	rm -f $(TESTING) ioctl_test
