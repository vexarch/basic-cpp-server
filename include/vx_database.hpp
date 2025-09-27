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
#include "helpers.h"

namespace fs = std::filesystem;

#define FRAME_SIZE 4096

enum class DataType {
    CHAR,
    WCHAR,
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
};

class Schema {
private:
    std::vector<column> columns;
public:
    Schema();
    Schema(const std::string& schema);
    Schema(const std::vector<column>& columns);

    // adds a new column to the table schema
    // The columns are added on the right of the existing
    // the size is on bytes
    void add_column(const std::string& name, const DataType type, const int count = 1);

    int get_row_size() const;
    std::vector<int> get_sizes() const;

    std::vector<column> get_columns() const;

    // returns the schema in simple format | column1 | column2 |...
    std::string get_schema() const;
};

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

    // Static constants
    static constexpr int MIN_FRAME_SIZE = 4096; // 4 KB
    static constexpr int MAX_FRAME_SIZE = 1048576; // 1 MB
    static constexpr int CACHE_LT_S = 15; // Cache life time in seconds
    static constexpr int METADATA_LENGTH = 2048; // 2 KB

    // General constant infos
    std::string name;
    std::string file_name;
    std::vector<column> columns;
    std::vector<padding> paddings;
    std::vector<int> sizes;
    int element_size;
    int frame_size;
    int frame_capacity;

    // Data variables
    std::fstream file;
    std::vector<std::unique_ptr<frame>> frames;
    int elements_count = 0;

    // Concurrency objects
    std::shared_mutex file_mutex;

    // Organizing functions
    //void arrange_frame(frame& f);
    //void order_table();

    // File I/O functions
    void initialize_file();
    void write_metadata();
    void read_metadata();
    frame& add_frame();
    void load_frame(frame& f);
    void unload_frame(frame& f);
    void flush_frame(frame& f);
    void flush_all();

public:
    // Used when file exists, it gets the schema and columns from the metadata in the begining of the file
    Table(const std::string& name);
    // When file exists or doesn't exist, could throw errors if schema and metadata aren't compatible
    Table(const std::string& name, const Schema& schema);
    // When file exists or doesn't exist, could throw errors if schema and metadata aren't compatible
    Table(const std::string& name, const std::vector<column>& columns);

    int get_rows_count() {
        return elements_count;
    }

    // These functions assume that the element e is a struct that follows the same schema as the table

    template <typename T>
    void add_element(const T& e) {
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        pack_struct(&e, buffer.get(), sizes, paddings);
        for (auto& f: frames) {
            if (f->count < frame_capacity) {
                load_frame(*f);
                std::unique_lock<std::shared_mutex> lock(f->mutex);
                char* ptr2 = f->data.get() + (f->count * element_size);
                for (char* ptr1 = buffer.get(); ptr1 < buffer.get() + element_size; ptr1++, ptr2++) {
                    *ptr2 = *ptr1;
                }
                elements_count++;
                f->count++;
                return;
            }
        }
        auto& f = add_frame();
        load_frame(f);
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        char* ptr2 = f.data.get() + (f.count * element_size);
        for (char* ptr1 = buffer.get(); ptr1 < buffer.get() + element_size; ptr1++, ptr2++) {
            *ptr2 = *ptr1;
        }
        elements_count++;
        f.count++;
    }

    template <typename T>
    inline void add_elements(const std::vector<T>& e) {
        for (auto& x: e) add_element(x);
    }

    template <typename T>
    T get_element(int index) {
        if (index > elements_count) throw std::out_of_range("Element index out of table range");
        int count = 0;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            count += f->count;
            if (index <= count) {
                T e;
                unpack_struct(f->data.get() + (count - index) , &e, sizes, paddings);
                return e;
            }
        }
        throw std::out_of_range("Element index out of table range");
    }

    // Could cause problems if the table contained too many rows
    template <typename T>
    std::vector<T> get_all() {
        std::vector<T> result(elements_count);
        int index = 0;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count; i++) {
                unpack_struct(f->data.get() + i * element_size, &result[i], sizes, paddings);
            }
        }
        return result;
    }

    template <typename T>
    T find_first(bool(*pred)(T)) {
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count; i++) {
                unpack_struct(f->data.get() + i * element_size, &e, sizes, paddings);
                if (pred(e)) return e;
            }
        }
    }

    template <typename T>
    T pop_first(bool(*pred)(T)) {
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (long)(ptr + element_size));
                    f->count--;
                    elements_count--;
                    return e;
                }
            }
        }
    }

    template <typename T>
    std::vector<T> find(bool(*pred)(T), int count = 1) {
        if (count < 0) throw std::invalid_argument("count cannot be less than 0");
        std::vector<T> result;
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count && result.size() < count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);
                if (pred(e)) result.push_back(e);
            }
            if (result.size() == count) break;
        }
        return result;
    }

    template <typename T>
    std::vector<T> pop(bool(*pred)(T), int count = 1) {
        if (count < 0) throw std::invalid_argument("count cannot be less than 0");
        std::vector<T> result;
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count && result.size() < count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (long)(ptr + element_size));
                    f->count--;
                    elements_count--;
                    result.push_back(e);
                }
            }
            if (result.size() == count) break;
        }
        return result;
    }

    template <typename T>
    void remove(bool(*pred)(T), int count = 1) {
        if (count < 0) throw std::invalid_argument("count cannot be less than 0");
        int c = 0;
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count && c < count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (long)(ptr + element_size));
                    f->count--;
                    elements_count--;
                    c++;
                }
            }
            if (c >= count) break;
        }
    }

    template <typename T>
    std::vector<T> find_all(bool(*pred)(T)) {
        std::vector<T> result;
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);
                if (pred(e)) result.push_back(e);
            }
        }
        return result;
    }

    template <typename T>
    std::vector<T> pop_all(bool(*pred)(T)) {
        std::vector<T> result;
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (long)(ptr + element_size));
                    f->count--;
                    elements_count--;
                    result.push_back(e);
                }
            }
        }
        return result;
    }

    template <typename T>
    void remove_all(bool(*pred)(T)) {
        T e;
        for (auto& f: frames) {
            std::shared_lock<std::shared_mutex> lock(f->mutex);
            load_frame(*f);
            for (int i = 0; i < f->count; i++) {
                char* ptr = f->data.get() + i * element_size;
                unpack_struct(ptr, &e, sizes, paddings);

                if (pred(e)) {
                    lock.unlock();
                    std::unique_lock<std::shared_mutex> lock2(f->mutex);
                    std::memmove(ptr, ptr + element_size, frame_size - (long)(ptr + element_size));
                    f->count--;
                    elements_count--;
                }
            }
        }
    }

    void clear();

    ~Table();
};

#endif
