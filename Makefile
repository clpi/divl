LLVM_FLAGS ?= `llvm-config \
							--cxxflags \
							--ldflags \
							--system-libs \
							--libs \
							core \
							native \
							orcjit`
CXX_FLAGS ?= -g -O3

SRC ?= src/div.cc
OUT ?= dist/
NAME ?= div
BIN ?= $(OUT)$(NAME)


d:
	rm -rf $(OUT) && mkdir -p $(OUT)
	$(CXX) $(SRC) $(LLVM_FLAGS) $(CXX_FLAGS) -o $(BIN)
	./$(BIN)
