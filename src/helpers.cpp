#include "helpers.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>

#include <iostream>
#include <filesystem>
#include <fstream>

#include <map>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "http.hpp"
#include "controller.h"
#include "ssl.h"

namespace fs = std::filesystem;

std::map<std::string, std::vector<char>> get_all_files(const std::string& dir) {
    std::map<std::string, std::vector<char>> files;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return files;
        // throw std::invalid_argument("Static files directory does not exist");
    }
    for (const auto & entry: fs::directory_iterator(dir)) {
        std::string path = entry.path();
        char* ptr = path.data() + path.size();
        while (*ptr != '/') {
            ptr--;
        }
        if (fs::is_regular_file(path)) {
            files[ptr] = std::vector<char>(fs::file_size(path));
            std::ifstream in(path, std::ios::binary);
            in.read(files[ptr].data(), files[ptr].size());
        } else if (fs::is_directory(path)) {
            for (auto file: get_all_files(path)) {
                files[ptr + file.first] = file.second;
            }
        }
    }
    return files;
}

std::string read_to_end(int fd, int timeout) {
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    int result = select(fd + 1, &readfds, nullptr, nullptr, &tv);

    if (result == -1) {
        throw runtime_error("Failed to select socket fd");
    } else if (result == 0) {
        throw runtime_error("Timeout occured");
    }

    if (FD_ISSET(fd, &readfds)) {
        std::vector<unsigned char> buffer(BUFFER_SIZE);
        std::string data;
        int bytes_read;

        do {
            bytes_read = read(fd, buffer.data(), BUFFER_SIZE);
            if (bytes_read < 0)
                throw std::runtime_error("Failed to read from file descriptor");
            data.append((char*)buffer.data(), bytes_read);

            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            result = select(fd + 1, &readfds, nullptr, nullptr, &tv);
            if (result == -1) {
                throw runtime_error("Failed to select socket fd");
            }
        } while (bytes_read == BUFFER_SIZE && FD_ISSET(fd, &readfds));

        return data;
    } else {
        throw runtime_error("Failed to read from file descriptor");
    }
}

std::string read_to_end(SSL* ssl, int fd, int timeout) {
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    int result = select(fd + 1, &readfds, nullptr, nullptr, &tv);

    if (result == -1) {
        throw runtime_error("Failed to select socket fd");
    } else if (result == 0) {
        throw runtime_error("Timeout occured");
    }

    if (FD_ISSET(fd, &readfds)) {
        std::vector<unsigned char> buffer(BUFFER_SIZE);
        std::string data;
        int bytes_read;

        do {
            bytes_read = SSL_read(ssl, data.data(), BUFFER_SIZE);
            if (bytes_read < 0)
                throw std::runtime_error("Failed to read from file descriptor");
            data.append((char*)buffer.data(), bytes_read);

            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            result = select(fd + 1, &readfds, nullptr, nullptr, &tv);
            if (result == -1) {
                throw runtime_error("Failed to select socket fd");
            }
        } while (bytes_read == BUFFER_SIZE && FD_ISSET(fd, &readfds));

        return data;
    } else {
        throw runtime_error("Failed to read from file descriptor");
    }
}

std::string get_content_type(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string extension = filename.substr(dotPos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    static const std::unordered_map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"txt", "text/plain"},
        {"csv", "text/csv"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"gz", "application/gzip"},
        {"tar", "application/x-tar"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"mp4", "video/mp4"},
        {"avi", "video/x-msvideo"},
        {"mov", "video/quicktime"},
        {"mkv", "video/x-matroska"},
        {"doc", "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xls", "application/vnd.ms-excel"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"ppt", "application/vnd.ms-powerpoint"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {"ttf", "font/ttf"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"}
    };

    auto it = mime_types.find(extension);
    if (it != mime_types.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

std::string get_time() {
    time_t now = time(nullptr);
    char buffer[26];
    ctime_r(&now, buffer);
    std::string str(buffer);
    int i = 0;
    int pos = 0;
    while (true) {
        if ((pos = (int)str.find(' ', pos)) != std::string::npos) {i++;pos++;}
        else return "";
        if (i == 3) {
            str = str.substr(pos, 8);
            break;
        }
    }

    return str;
}

std::string ip_to_str(int ip) {
    std::ostringstream oss;
    unsigned char* ptr = (unsigned char*)&ip;
    oss << (int)*ptr << "." << (int)*(++ptr) << "." << (int)*(++ptr) << "." << (int)*(++ptr);
    return oss.str();
}
