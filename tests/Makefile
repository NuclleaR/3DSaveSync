# PC unit tests for source/core. Run with: make -C tests
CXXFLAGS := -std=c++23 -Wall -Wextra -Werror -I../source -Ithird_party -MMD -MP
LDLIBS   := -ltinyxml2

CORE_SRC := $(wildcard ../source/core/*.cpp)
TEST_SRC := $(wildcard *.cpp)
OBJS     := $(patsubst ../source/core/%.cpp,build/core/%.o,$(CORE_SRC)) \
            $(patsubst %.cpp,build/%.o,$(TEST_SRC))

.PHONY: all clean

all: build/run_tests
	./build/run_tests

build/run_tests: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/core/%.o: ../source/core/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf build

-include $(OBJS:.o=.d)
