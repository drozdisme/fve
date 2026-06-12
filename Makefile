CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unknown-pragmas -frounding-math -ffp-contract=off -I.
COVFLAGS = -O0 -g --coverage -fno-inline

SRC = $(wildcard core/*.cpp core/*/*.cpp extractor/*.cpp extractor/*/*.cpp oracle/*.cpp model/*/*.cpp semantic/*.cpp wrapper/*.cpp registry/*.cpp audit/*.cpp db/*.cpp platform/*.cpp api/*.cpp l0/*.cpp casebase/*.cpp)
OBJ = $(SRC:.cpp=.o)
TESTSRC = $(wildcard tests/*.cpp)

LIB = libcore.a
BIN = bin/verify
EXTRACT = bin/extract
SERVER = bin/server
CRAWL = bin/crawl
FIND = bin/find
WATCH = bin/watch
FVERUN = bin/fve-run
TESTBIN = bin/run_tests

.PHONY: all test coverage clean

all: $(LIB) $(BIN) $(EXTRACT) $(SERVER) $(CRAWL) $(FIND) $(WATCH) $(FVERUN)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB): $(OBJ)
	ar rcs $@ $(OBJ)

$(BIN): cmd/verify.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

$(EXTRACT): cmd/extract.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

$(WATCH): cmd/watch.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

$(FIND): cmd/find.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

$(CRAWL): cmd/crawl.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

$(SERVER): cmd/server.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -pthread $< $(LIB) -o $@

$(FVERUN): cmd/fve-run.cpp $(LIB)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -pthread $< $(LIB) -o $@

$(TESTBIN): $(TESTSRC) $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(TESTSRC) $(SRC) -o $@

test: $(TESTBIN)
	./$(TESTBIN)

coverage:
	@mkdir -p bin
	$(CXX) -std=c++17 $(COVFLAGS) -ffp-contract=off -I. $(TESTSRC) $(SRC) -o bin/cov_tests
	./bin/cov_tests
	@gcov -r $(SRC) >/dev/null 2>&1 || true
	@echo "coverage objects written (*.gcov); see docs/BUILD.md"

clean:
	rm -f $(OBJ) $(LIB) $(BIN) $(EXTRACT) $(SERVER) $(CRAWL) $(FIND) $(TESTBIN) bin/cov_tests *.gcov *.gcda *.gcno
	rm -f $(shell find core extractor oracle model semantic wrapper registry audit db platform api l0 -name '*.gc*' 2>/dev/null)
	rm -rf bin
