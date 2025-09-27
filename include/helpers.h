#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <map>
#include <vector>
#include <openssl/ssl.h>

#define BUFFER_SIZE 16384

struct padding {
    int offset = 0;
    int size = 0;
};

// Read the content of all files from the directory dir
std::map<std::string, std::vector<char>> get_all_files(const std::string& dir);

// Reads data using a file descriptor untile no data left
// timeout in seconds
std::string read_to_end(int fd, int timeout = 10);

std::string read_to_end(SSL* ssl, int fd, int timeout = 10);

// returns the MIME content type
std::string get_content_type(const std::string& filename);

// returns the current time in HH:MM:SS format
std::string get_time();

std::string ip_to_str(int ip);

// Copies from a struct with padding to a tightly packed buffer (no padding between members)
// Arguments:
//   src: pointer to the beginning of the source struct (with padding)
//   dst: pointer to the beginning of the destination packed buffer
//   memberSizes: vector of sizes of each member in order
//   paddings: precomputed paddings (including final padding if any)
void pack_struct(const void* src, void* dst, const std::vector<int>& memberSizes, const std::vector<padding>& paddings);

// Copies from a tightly packed buffer to a struct with padding
// Arguments:
//   src: pointer to packed buffer (no padding)
//   dst: pointer to destination struct (with padding)
//   memberSizes: vector of sizes of each member in order
//   paddings: precomputed paddings (including final padding if any)
void unpack_struct(const void* src, void* dst, const std::vector<int>& memberSizes, const std::vector<padding>& paddings);

#endif // HELPERS_H
