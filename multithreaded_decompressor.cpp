#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>   // For uint32_t
#include <stdexcept> // For std::runtime_error
#include <zlib.h>    // Requires linking with -lz

// Define a constant for the chunk size.
// This MUST match the CHUNK_SIZE used by the compressor to ensure
// the decompression buffer is large enough.
const size_t CHUNK_SIZE = 1024 * 1024; // 1 MB

// Decompresses a vector of data using zlib's uncompress function.
// It assumes the uncompressed data for a single chunk will not exceed CHUNK_SIZE.
std::vector<unsigned char> decompressData(const std::vector<unsigned char>& input) {
    if (input.empty()) {
        return {};
    }

    // Create a destination buffer for the uncompressed data.
    // We allocate a buffer of the original max chunk size.
    std::vector<unsigned char> output(CHUNK_SIZE);
    uLongf destLen = output.size();

    // Perform the decompression.
    // uncompress is a zlib function that decompresses a source buffer into a destination buffer.
    int result = uncompress(output.data(), &destLen, input.data(), input.size());

    if (result != Z_OK) {
        // Handle potential errors. Z_BUF_ERROR might mean the destination buffer
        // was too small, which shouldn't happen if CHUNK_SIZE is consistent.
        // Other errors might indicate corrupt data.
        throw std::runtime_error("Decompression failed with zlib error: " + std::to_string(result));
    }

    // Resize the output vector to the actual size of the decompressed data.
    output.resize(destLen);
    return output;
}

int main(int argc, char* argv[]) {
    // Check for the correct number of command-line arguments.
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <compressed_input_file> <output_file>\n";
        std::cerr << "Example: " << argv[0] << " compressed.dat output.txt\n";
        return 1;
    }

    // Open the compressed input file in binary mode.
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Error: Could not open input file " << argv[1] << "\n";
        return 1;
    }

    // Open the destination output file in binary mode.
    std::ofstream out(argv[2], std::ios::binary);
    if (!out) {
        std::cerr << "Error: Could not open output file " << argv[2] << "\n";
        return 1;
    }

    std::cout << "Starting decompression...\n";

    // Loop through the file as long as we haven't reached the end.
    // in.peek() checks the next character without extracting it.
    while (in.peek() != EOF) {
        // --- Step 1: Read the size of the next compressed chunk ---
        uint32_t compressedChunkSize;
        in.read(reinterpret_cast<char*>(&compressedChunkSize), sizeof(compressedChunkSize));

        // Check if we successfully read the size.
        if (in.gcount() != sizeof(compressedChunkSize)) {
            if (in.eof()) {
                // This can happen if the file ends cleanly on a chunk boundary.
                break; 
            }
            std::cerr << "Error: Failed to read chunk size. File may be corrupt.\n";
            return 1;
        }

        // --- Step 2: Read the compressed chunk data ---
        std::vector<unsigned char> compressedData(compressedChunkSize);
        in.read(reinterpret_cast<char*>(compressedData.data()), compressedChunkSize);

        if (in.gcount() != compressedChunkSize) {
            std::cerr << "Error: Failed to read chunk data. File may be corrupt or truncated.\n";
            return 1;
        }

        // --- Step 3: Decompress the chunk ---
        try {
            std::vector<unsigned char> decompressedData = decompressData(compressedData);
            
            // --- Step 4: Write the decompressed data to the output file ---
            out.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
        } catch (const std::runtime_error& e) {
            std::cerr << "An error occurred during decompression: " << e.what() << '\n';
            return 1;
        }
    }

    // Close the file streams.
    in.close();
    out.close();

    std::cout << "File decompression successful. Output written to " << argv[2] << ".\n";

    return 0;
}
