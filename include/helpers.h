#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <map>
#include <vector>
#include <openssl/ssl.h>

#define BUFFER_SIZE 16384

// Read the content of all files from the directory dir
std::map<std::string, std::vector<char>> get_all_files(const std::string& dir);

// Reads data using a file descriptor untile no data left
// timeout in seconds
std::string read_to_end(int fd, int timeout = 10);

std::string read_to_end(SSL* ssl, int fd, int timeout = 10);

// returns the MIME content type
std::string get_content_type(const std::string& filename);

#endif // HELPERS_H
