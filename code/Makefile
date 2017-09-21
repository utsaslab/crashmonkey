GCC = gcc
GPP = g++
DEBUG_FLAGS += -g -DDEBUG
GOPTS = -std=c++11
GOTPSSO = -shared -fPIC

BUILD_DIR = $(CURDIR)/../build
TESTING := testing/log_on_off testing/test_get_log_ent_size

MODULES = cow_brd disk_wrapper
obj-m := $(addsuffix .o, $(MODULES))
KDIR := /lib/modules/$(shell uname -r)/build

CM_TESTS = $(patsubst %.cpp, %.so, $(notdir $(wildcard $(CURDIR)/tests/*.cpp)))
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
		$(foreach MOD, $(MODULES), $(addsuffix .ko, $(BUILD_DIR)/$(MOD)))

c_harness: \
		$(BUILD_DIR)/c_harness

user_tools: \
		$(BUILD_DIR)/user_tools/begin_log \
		$(BUILD_DIR)/user_tools/end_log \
		$(BUILD_DIR)/user_tools/begin_tests \
		$(BUILD_DIR)/user_tools/cm_checkpoint

tests: \
		$(foreach TEST, $(CM_TESTS), $(BUILD_DIR)/tests/$(TEST))

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

$(BUILD_DIR)/%.ko: \
		%.c \
		$(BUILD_DIR)/Makefile
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) src=$(CURDIR) modules

$(BUILD_DIR)/harness/%.o: \
		harness/%.cpp
	mkdir -p $(@D)
	$(GPP) -std=c++11 $^ -c -o $@

$(BUILD_DIR)/c_harness: \
		harness/c_harness.cpp \
		harness/Tester.cpp \
		$(BUILD_DIR)/utils/utils.o \
		$(BUILD_DIR)/utils/utils_c.o \
		$(BUILD_DIR)/utils/communication/ServerSocket.o \
		$(BUILD_DIR)/utils/communication/BaseSocket.o \
		$(BUILD_DIR)/permuter/Permuter.o \
		$(BUILD_DIR)/results/TestSuiteResult.o \
		$(BUILD_DIR)/results/FileSystemTestResult.o \
		$(BUILD_DIR)/results/DataTestResult.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $^ -ldl -o $@

$(BUILD_DIR)/tests/%.so: \
		tests/%.cpp \
		$(BUILD_DIR)/user_tools/src/actions.o \
		$(BUILD_DIR)/utils/communication/ClientCommandSender.o \
		$(BUILD_DIR)/results/DataTestResult.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,$(notdir $@) -o $@ $^

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
		$(BUILD_DIR)/utils/utils.o \
		$(BUILD_DIR)/utils/utils_c.o
	mkdir -p $(@D)
	$(GPP) $(GOPTS) $(GOTPSSO) -Wl,-soname,RandomPermuter.so \
		-o $(BUILD_DIR)/permuter/RandomPermuter.so $^

$(BUILD_DIR)/utils/utils.o: \
		utils/utils.cpp
	mkdir -p $(@D)
	$(GPP) $(GOPTS) -fPIC -c -o $@ $<

$(BUILD_DIR)/utils/utils_c.o: \
		utils/utils_c.c
	mkdir -p $(@D)
	$(GCC) -c -o $@ $< -I /usr/src/linux-headers-$(shell uname -r)/include/

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
