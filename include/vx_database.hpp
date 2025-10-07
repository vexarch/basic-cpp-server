#ifndef VX_DATABASE_H
#define VX_DATABASE_H

#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <stdexcept>
#include "helpers.h"

namespace fs = std::filesystem;

enum class DataType {
    CHAR,
    STRING,
    INT8,
    INT16,
    INT32,
    INT64,
    FLOAT32,
    FLOAT64,
};

struct column {
    std::string name;
    DataType type;
    int count = 1; // Used for array cells

    bool operator==(const column& c) const {
        return c.type == this->type && c.count == this->count && c.name == this->name;
    }
};

class Schema {
private:
    std::vector<column> columns;
    std::vector<padding> paddings;
    std::vector<int> sizes;
    std::vector<int> strings_offsets;
    int row_size = 0;
    bool has_strings = false;

    void calculate_row_size();
    void calculate_strings_offsets();
    void calculate_sizes();
    void calculate_paddings();
public:
    Schema();
    Schema(const std::string& schema);
    Schema(const std::vector<column>& columns);

    // adds a new column to the table schema
    // The columns are added on the right of the existing
    // the size is on bytes
    void add_column(const std::string& name, const DataType type, const int count = 1);

    int get_row_size() const;
    std::vector<int> get_strings_offsets() const;
    std::vector<int> get_sizes() const;
    std::vector<column> get_columns() const;
    std::vector<padding> get_paddings() const;
    bool contain_strings() const;

    // Copies from a struct with padding to a tightly packed buffer (no padding between members)
    // Arguments:
    //   src: pointer to the beginning of the source struct (with padding)
    //   dst: pointer to the beginning of the destination packed buffer
    //   memberSizes: vector of sizes of each member in order
    //   paddings: precomputed paddings (including final padding if any)
    void pack_struct(const void* src, void* dst);

    // Copies from a tightly packed buffer to a struct with padding
    // Arguments:
    //   src: pointer to packed buffer (no padding)
    //   dst: pointer to destination struct (with padding)
    //   memberSizes: vector of sizes of each member in order
    //   paddings: precomputed paddings (including final padding if any)
    void unpack_struct(const void* src, void* dst);

    // returns the schema in simple format | column1 | column2 |...
    std::string get_schema() const;
};

template<typename T>
class TypedTable;

class Table {
private:
    struct frame {
        int count; // the number of existing elements
        int file_pos; // position in the file
        std::unique_ptr<char[]> data;
        std::shared_mutex mutex;
        std::atomic<bool> accessed{false};

        frame() = default;

        frame(const frame&) = delete;
        frame& operator=(const frame&) = delete;
        frame(frame&&) = delete;
        frame& operator=(frame&&) = delete;
    };

    struct query_result {
        int rows_affected = 0;
        int rows_added = 0;
        int rows_deleted = 0;
        int rows_updated = 0;

        int query_data_count = 0;
        std::unique_ptr<char[]> query_data;
    };

    // Static constants
    static constexpr int MIN_FRAME_SIZE = 4096; // 4 KB
    static constexpr int MAX_FRAME_SIZE = 1048576; // 1 MB
    static constexpr int CACHE_LT_S = 5; // Cache life time in seconds
    static constexpr int METADATA_LENGTH = 2048; // 2 KB

    // General constant infos
    std::string name;
    std::string file_name;
    std::string strings_file_name;
    Schema schema;
    std::vector<column> columns;
    int element_size;
    int frame_size;
    int frame_capacity;

    // Data variables
    std::fstream file;
    std::fstream strings_file;
    std::vector<std::unique_ptr<frame>> frames;
    int elements_count = 0;

    // Concurrency objects
    std::shared_mutex file_mutex;
    std::shared_mutex strings_file_mutex;

    // Organizing functions
    //void arrange_frame(frame& f);
    //void order_table();

    // File I/O functions
    void initialize_file();
    void write_metadata();
    void read_metadata();
    void add_frame();
    void load_frame(frame& f);
    void unload_frame(frame& f);
    void flush_frame(frame& f);
    void flush_all();

    // Management functions
    void add(void* buffer, int count = 1);
    void* get_at(int index);

    char* add_string(const char* str, const int len);
    inline char* add_string(const std::string& str) {
        return add_string(str.data(), (int)str.length());
    }
    char* get_string(const char* ptr, const int len);
    void remove_string(const char* ptr, const int len);

    template<typename T>
    friend class TypedTable;

public:
    // Used when file exists, it gets the schema and columns from the metadata in the begining of the file
    Table(const std::string& name);
    // When file exists or doesn't exist, could throw errors if schema and metadata aren't compatible
    Table(const std::string& name, const Schema& schema);
    // When file exists or doesn't exist, could throw errors if schema and metadata aren't compatible
    Table(const std::string& name, const std::vector<column>& columns);

    int get_rows_count() const {
        return elements_count;
    }

    /*
     * e is a string that represent a row (or multiple rows)
     * the syntax is : col1, col2, col3, ... (one row)
     *                 (col1, col2, col3, ...), (col1, col2, col3, ...), ... (multiple rows)
     * Warning!: be aware when using this function, it doesn't have strong syntax checking and could cause unpredictable effects on the table data
     */
    query_result add(const std::string& e);

    // con is a string that represent the conditions
    // the syntax is : col1 == val1 && col2 != val2 || col3 > 0 ...
    query_result find(const std::string& con) {
        throw std::logic_error("Not implemented");
    }

    // con is a string that represent the conditions
    // the syntax is : col1 == val1 && col2 != val2 || col3 > 0 ...
    query_result pop(const std::string& con) {
        throw std::logic_error("Not implemented");
    }

    // con is a string that represent the conditions
    // the syntax is : col1 == val1 && col2 != val2 || col3 > 0 ...
    query_result remove(const std::string& con) {
        throw std::logic_error("Not implemented");
    }

    void clear();

    ~Table();
};

template<typename T>
class TypedTable : public Table {
public:
    // Used when file exists, it gets the schema and columns from the metadata in the begining of the file
    TypedTable(const std::string& name): Table(name) {}
    // When file exists or doesn't exist, could throw errors if schema and metadata aren't compatible
    TypedTable(const std::string& name, const Schema& schema): Table(name, schema) {}
    // When file exists or doesn't exist, could throw errors if schema and metadata aren't compatible
    TypedTable(const std::string& name, const std::vector<column>& columns): Table(name, columns) {}
    // These functions assume that the type T is a struct that follows the same schema as the table

    void add_element(const T& e) {
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        //pack_struct(&e, buffer.get(), sizes, paddings);
        schema.pack_struct(&e, buffer.get());
        add(buffer.get());
    }

    void add_elements(const std::vector<T>& e) {
        for (auto& x: e) add_element(x);
    }

    T get_element(int index) {
        if (index > elements_count || index < 0) throw std::out_of_range("Element index out of table range");
        std::unique_ptr<char[]> buffer;
        buffer.reset((char*)get_at(index));
        if (!buffer)
            std::runtime_error("Failed to get element at index " + std::to_string(index));
        T e;
        schema.unpack_struct(buffer.get(), &e);
        for (auto& s: schema.get_strings_offsets()) {
            char* p = *(char**)(buffer.get() + s + 4);
            if (p != nullptr)
                free(p);
        }
        return e;
        throw std::out_of_range("Element index out of table range");
    }

    // Could cause problems if the table contained too many rows
    std::vector<T> get_all() {
        std::vector<T> result(elements_count);
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }
                result[i] = e;
                ptr += element_size;
            }
        }
        return result;
    }

    T find_first(std::function<bool(T)> pred) {
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }
                if (pred(e)) return e;
                ptr += element_size;
            }
        }
        throw std::runtime_error("Cannot find the element");
    }

    T pop_first(std::function<bool(T)> pred) {
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (long)(ptr + element_size));
                    f->count--;
                    elements_count--;
                    return e;
                }
                ptr += element_size;
            }
        }
    }

    std::vector<T> find(std::function<bool(T)> pred, int count = 1) {
        if (count < 0) throw std::invalid_argument("count cannot be less than 0");
        else if (count == 0) return {};
        std::vector<T> result(count);
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        int indexer = 0;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count && result.size() < count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) result[indexer++] = e;
                else ptr += element_size;
                if (indexer >= count) return result;
            }
        }
        return result;
    }

    std::vector<T> pop(std::function<bool(T)> pred, int count = 1) {
        if (count < 0) throw std::invalid_argument("count cannot be less than 0");
        else if (count == 0) return {};
        std::vector<T> result(count);
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        int indexer = 0;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count && result.size() < count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (i * element_size));
                    lock2.unlock();
                    f->count--;
                    elements_count--;
                    result[indexer++] = e;
                    lock.lock();
                } else
                    ptr += element_size;
                if (indexer >= count) return result;
            }
        }
        return result;
    }

    void remove(std::function<bool(T)> pred, int count = 1) {
        if (count < 0) throw std::invalid_argument("count cannot be less than 0");
        else if (count == 0) return;
        int c = 0;
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count && c < count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (i * element_size));
                    lock2.unlock();
                    f->count--;
                    elements_count--;
                    c++;
                    lock.lock();
                } else
                    ptr += element_size;
                if (c >= count) break;
            }
        }
    }

    std::vector<T> find_all(std::function<bool(T)> pred) {
        std::vector<T> result;
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) result.push_back(e);
                else ptr += element_size;
            }
        }
        return result;
    }

    std::vector<T> pop_all(std::function<bool(T)> pred) {
        std::vector<T> result;
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (i * element_size));
                    lock2.unlock();
                    f->count--;
                    elements_count--;
                    result.push_back(e);
                    lock.lock();
                } else
                    ptr += element_size;
            }
        }
        return result;
    }

    void remove_all(std::function<bool(T)> pred) {
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        T e;
        for (auto& f: frames) {
            load_frame(*f);
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            char* ptr = f->data.get();
            for (int i = 0; i < f->count; i++) {
                if (schema.contain_strings()) {
                    std::memcpy(buffer.get(), ptr, element_size);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* str = get_string(*(char**)(buffer.get() + s + 4), *(int*)(buffer.get() + s));
                        *(char**)(buffer.get() + s + 4) = str;
                    }
                    schema.unpack_struct(buffer.get(), &e);
                    for (auto& s: schema.get_strings_offsets()) {
                        char* p = *(char**)(buffer.get() + s + 4);
                        if (p != nullptr)
                            free(p);
                    }
                } else {
                    schema.unpack_struct(ptr, &e);
                }

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (i * element_size));
                    lock2.unlock();
                    f->count--;
                    elements_count--;
                    lock.lock();
                } else
                    ptr += element_size;
            }
        }
    }
};

#endif
