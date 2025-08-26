## Description
High-speed multithreaded File Compressor in C++. Splits the input file into chunks and assigns worker threads to each chunk for parrellel compression. Also included in a decompressor. 

## How to Run
Compile: `make`  
To compress: `./compressor targetFile outputFile`  
To decompress: `./decompressor targetFile outputFile`
