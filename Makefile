CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall
LDFLAGS := -lz -pthread

# Executable names
COMPRESSOR := compressor
DECOMPRESSOR := decompressor

# Source files
COMPRESSOR_SRC := multithreaded_compressor.cpp
DECOMPRESSOR_SRC := decompressor.cpp

.PHONY: all clean

# Build both programs
all: $(COMPRESSOR) $(DECOMPRESSOR)

$(COMPRESSOR): $(COMPRESSOR_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(DECOMPRESSOR): $(DECOMPRESSOR_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(COMPRESSOR) $(DECOMPRESSOR)