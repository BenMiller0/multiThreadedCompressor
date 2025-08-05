#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <algorithm> // For std::sort
#include <stdexcept> // For std::runtime_error
#include <zlib.h>    // Requires linking with -lz

// Define a constant for the chunk size (1MB).
const size_t CHUNK_SIZE = 1024 * 1024;

// Represents a chunk of data read from the input file.
struct Chunk {
    size_t id;
    std::vector<unsigned char> data;
};

// Represents a chunk of data after compression.
struct CompressedChunk {
    size_t id;
    std::vector<unsigned char> data;
};

// A simple and robust thread pool implementation.
class ThreadPool {
public:
    // Constructor: creates a specified number of worker threads.
    ThreadPool(size_t n) : stop(false) {
        for (size_t i = 0; i < n; ++i)
            workers.emplace_back([this]() { this->worker_thread(); });
    }

    // Destructor: ensures the thread pool is shut down properly.
    ~ThreadPool() {
        shutdown();
    }

    // Enqueues a new task for the workers to execute.
    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stop) {
                // Do not enqueue new tasks if the pool is stopping.
                return;
            }
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    // Shuts down the thread pool, waiting for all tasks to complete.
    void shutdown() {
        if (stop) return; // Already shutting down
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

    // The main loop for each worker thread.
    void worker_thread() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                if (this->stop && this->tasks.empty()) {
                    return;
                }
                task = std::move(tasks.front());
                tasks.pop();
            }
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "Exception caught in worker thread: " << e.what() << '\n';
            }
        }
    }
};

// Compresses a vector of data using zlib.
std::vector<unsigned char> compressData(const std::vector<unsigned char>& input) {
    if (input.empty()) {
        return {};
    }
    // Calculate the upper bound for the compressed data size.
    uLongf compressedSize = compressBound(input.size());
    std::vector<unsigned char> output(compressedSize);

    // Perform compression.
    if (compress(output.data(), &compressedSize, input.data(), input.size()) != Z_OK) {
        throw std::runtime_error("Compression failed");
    }

    // Resize the output vector to the actual compressed size.
    output.resize(compressedSize);
    return output;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>\n";
        return 1;
    }

    // Open input file for reading in binary mode.
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Error: Could not open input file " << argv[1] << "\n";
        return 1;
    }

    // Open output file for writing in binary mode.
    std::ofstream out(argv[2], std::ios::binary);
    if (!out) {
        std::cerr << "Error: Could not open output file " << argv[2] << "\n";
        return 1;
    }

    // --- Phase 1: Read the entire file into chunks ---
    std::vector<Chunk> chunks;
    size_t id_counter = 0;
    // Use a more robust loop for reading the file.
    std::vector<unsigned char> buffer(CHUNK_SIZE);
    while (in.read(reinterpret_cast<char*>(buffer.data()), CHUNK_SIZE)) {
        chunks.push_back({id_counter++, std::vector<unsigned char>(buffer.begin(), buffer.end())});
    }
    // Handle the last, potentially smaller, chunk.
    if (in.gcount() > 0) {
        buffer.resize(in.gcount());
        chunks.push_back({id_counter++, std::move(buffer)});
    }
    in.close();

    if (chunks.empty()) {
        std::cout << "Input file is empty. Nothing to compress.\n";
        return 0;
    }

    std::cout << "Read " << chunks.size() << " chunks from the input file.\n";

    // --- Phase 2: Compress all chunks in parallel ---
    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<CompressedChunk> compressed_chunks;
    std::mutex results_mutex;

    for (const auto& chunk : chunks) {
        pool.enqueue([&results_mutex, &compressed_chunks, chunk] {
            auto compressed_data = compressData(chunk.data);
            
            // Lock the mutex to safely add the result to the shared vector.
            std::lock_guard<std::mutex> lock(results_mutex);
            compressed_chunks.push_back({chunk.id, std::move(compressed_data)});
        });
    }

    // Shutdown the pool. This call will block until all enqueued compression tasks are complete.
    // This is the main synchronization point that fixes the deadlock.
    std::cout << "Compressing chunks...\n";
    pool.shutdown();
    std::cout << "Compression complete.\n";

    // --- Phase 3: Sort and write the compressed chunks ---
    std::cout << "Sorting compressed chunks...\n";
    std::sort(compressed_chunks.begin(), compressed_chunks.end(), 
              [](const CompressedChunk& a, const CompressedChunk& b) {
        return a.id < b.id;
    });

    std::cout << "Writing to output file...\n";
    for (const auto& compressed_chunk : compressed_chunks) {
        // We also need to write the size of the chunk so we can decompress it later.
        uint32_t size = compressed_chunk.data.size();
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        out.write(reinterpret_cast<const char*>(compressed_chunk.data.data()), size);
    }
    out.close();

    std::cout << "File compression successful.\n";

    return 0;
}
