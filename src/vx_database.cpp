#include "vx_database.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <vector>
#include <list>
#include <filesystem>
#include <functional>
#include <algorithm>

namespace fs = std::filesystem;

Schema::Schema() {}

Schema::Schema(const std::string& schema) {
    // |column1|column2|column3[123]|...
    std::vector<std::string> names;
    int beg = 0;
    int end = 0;
    int index = 0;
    while (true) {
        if ((beg = schema.find('|', end)) == std::string::npos) break;
        beg++;
        if ((end = schema.find('|', beg)) == std::string::npos) break;
        names.push_back(schema.substr(beg, end - beg));
    }
    for (auto& name: names) {
        if ((index = name.find(':', 0)) == std::string::npos || index == 0)
            throw std::invalid_argument("Invalid schema string");
        column c;
        c.name = name.substr(0, index);
        std::string type;
        if ((beg = name.find('[', index)) != std::string::npos) {
            type = name.substr(index + 1, beg - index - 1);
            beg++;
            if ((end = name.find(']', beg)) == std::string::npos)
                throw std::invalid_argument("Invalid schema string");
            if (end - beg != 0) {
                std::string count = name.substr(beg, end - beg);
                c.count = std::stoi(count);
            }
        } else {
            type = name.substr(index + 1);
        }

        std::transform(type.begin(), type.end(), type.begin(), ::toupper);
        if (type == "CHAR") c.type = DataType::CHAR;
        else if (type == "STRING") {c.type = DataType::STRING; has_strings = true;}
        else if (type == "INT8") c.type = DataType::INT8;
        else if (type == "INT16") c.type = DataType::INT16;
        else if (type == "INT32") c.type = DataType::INT32;
        else if (type == "INT64") c.type = DataType::INT64;
        else if (type == "FLOAT32") c.type = DataType::FLOAT32;
        else if (type == "FLOAT64") c.type = DataType::FLOAT64;
        else throw std::invalid_argument("Invalid schema string: unknown type: " + type);

        if (c.count < 1) throw std::invalid_argument("Column count must be greater than 0");
        for (auto& x: columns)
            if (x.name == c.name)
                throw std::invalid_argument("Column with the name " + c.name + " already exists");
        columns.push_back(c);
    }

    calculate_row_size();
    calculate_strings_offsets();
    calculate_sizes();
    calculate_paddings();
}

Schema::Schema(const std::vector<column>& columns): columns(columns) {
    calculate_row_size();
    calculate_strings_offsets();
    calculate_sizes();
    calculate_paddings();
}

void Schema::calculate_row_size() {
    row_size = 0;
    for (auto& c: columns) {
        int member_size;
        switch (c.type) {
        case DataType::INT16:
            member_size = 2;
            break;
        case DataType::INT32:
        case DataType::FLOAT32:
            member_size = 4;
            break;
        case DataType::INT64:
        case DataType::FLOAT64:
            member_size = 8;
            break;
        case DataType::STRING:
            member_size = 12;
            break;
        default:
            member_size = 1;
            break;
        }
        row_size += c.count * member_size;
    }
}

void Schema::calculate_strings_offsets() {
    strings_offsets.clear();
    int offset = 0;
    for (auto& c: columns) {
        if (c.type == DataType::STRING) {
            for (int i = 0; i < c.count; i++) {
                strings_offsets.push_back(offset + (i * 12));
            }
        }
        int member_size;
        switch (c.type) {
        case DataType::INT16:
            member_size = 2;
            break;
        case DataType::INT32:
        case DataType::FLOAT32:
            member_size = 4;
            break;
        case DataType::INT64:
        case DataType::FLOAT64:
            member_size = 8;
            break;
        case DataType::STRING:
            member_size = 12;
            break;
        default:
            member_size = 1;
            break;
        }
        offset += c.count * member_size;
    }
}

void Schema::calculate_sizes() {
    sizes.clear();
    for (auto& c: columns) {
        int member_size;
        switch (c.type) {
        case DataType::INT16:
            member_size = 2;
            break;
        case DataType::INT32:
        case DataType::FLOAT32:
            member_size = 4;
            break;
        case DataType::INT64:
        case DataType::FLOAT64:
            member_size = 8;
            break;
        case DataType::STRING:
            member_size = 12;
            break;
        default:
            member_size = 1;
            break;
        }
        sizes.push_back(member_size * c.count);
    }
}

void Schema::calculate_paddings() {
    int current_offset = 0;
    int max_alignment = 1;

    for (auto& c: columns) {
        int alignment_requirement = 1;
        switch (c.type) {
        case DataType::INT16:
            alignment_requirement = 2;
            break;
        case DataType::INT32:
        case DataType::FLOAT32:
            alignment_requirement = 4;
            break;
        case DataType::INT64:
        case DataType::FLOAT64:
            alignment_requirement = 8;
            break;
        case DataType::STRING:
            alignment_requirement = sizeof(std::string);
            break;
        default:
            alignment_requirement = 1;
            break;
        }

        int alignment = alignment_requirement < 8 ? alignment_requirement : 8;

        max_alignment = max_alignment > alignment ? max_alignment : alignment;

        int aligned_offset = (current_offset + alignment - 1) / alignment * alignment;

        if (aligned_offset > current_offset)
            paddings.push_back({current_offset, aligned_offset - current_offset});

        current_offset = aligned_offset + (alignment_requirement * c.count);
    }

    int struct_size = (current_offset + max_alignment - 1) / max_alignment * max_alignment;
    if (struct_size > current_offset)
        paddings.push_back({current_offset, struct_size - current_offset});
}

void Schema::add_column(const std::string &name, const DataType type, const int count) {
    if (name.empty())
        throw std::invalid_argument("Column name must not be empty");
    for (const column &c : columns)
        if (c.name == name)
            throw std::invalid_argument("Column with the name " + name + " already exists");
    column c;
    c.name = name;
    c.type = type;
    if (count < 1)
        throw std::invalid_argument("Column count must be greater than 0");
    c.count = count;
    columns.push_back(c);

    calculate_row_size();
    calculate_strings_offsets();
    calculate_sizes();
    calculate_paddings();

    if (type == DataType::STRING) has_strings = true;
}

int Schema::get_row_size() const {
    return row_size;
}

std::vector<int> Schema::get_strings_offsets() const {
    return strings_offsets;
}

std::vector<int> Schema::get_sizes() const {
    return sizes;
}

std::vector<column> Schema::get_columns() const {
    return columns;
}

std::vector<padding> Schema::get_paddings() const {
    return paddings;
}

bool Schema::contain_strings() const {
    return has_strings;
}

void Schema::pack_struct(const void* src, void* dst) {
    const char* src_ptr = static_cast<const char*>(src);
    char* dst_ptr = static_cast<char*>(dst);

    int src_offset = 0;
    int padding_index = 0;

    for (int i = 0; i < sizes.size(); i++) {
        // Skip padding before this member
        if (padding_index < paddings.size() && paddings[padding_index].offset == src_offset) {
            src_offset += paddings[padding_index].size;
            padding_index++;
        }

        // Copy the member
        if (columns[i].type == DataType::STRING) {
            for (int j = 0; j < columns[i].count; j++) {
                int length = ((std::string*)(src_ptr + src_offset))->length();
                const char* ptr = ((std::string*)(src_ptr + src_offset))->data();

                *(int*)dst_ptr = length;
                //std::memcpy(dst_ptr, &length, 4);
                dst_ptr += 4;
                //std::memcpy(dst_ptr, &ptr, 8);
                *(const char**)dst_ptr = ptr;
                dst_ptr += 8;

                src_offset += sizeof(std::string);
            }
        } else {
            std::memcpy(dst_ptr, src_ptr + src_offset, sizes[i]);

            // Advance pointers
            dst_ptr += sizes[i];
            src_offset += sizes[i];
        }
    }
}

void Schema::unpack_struct(const void* src, void* dst) {
    const char* src_ptr = static_cast<const char*>(src);
    char* dst_ptr = static_cast<char*>(dst);

    int dst_offset = 0;
    int padding_index = 0;

    for (int i = 0; i < sizes.size(); ++i) {
        // Insert padding if needed before this member
        if (padding_index < paddings.size() && paddings[padding_index].offset == dst_offset) {
            // Skip over padding in destination (leave uninitialized or zero if you prefer)
            dst_offset += paddings[padding_index].size;
            ++padding_index;
        }

        // Copy the member
        if (columns[i].type == DataType::STRING) {
            for (int j = 0; j < columns[i].count; j++) {
                int length = *(int*)src_ptr;
                src_ptr += 4;
                char* ptr = *(char**)src_ptr;
                src_ptr += 8;

                if (ptr)
                    *(std::string*)(dst_ptr + dst_offset) = std::string(ptr, length);
                else
                    *(std::string*)(dst_ptr + dst_offset) = std::string();
                dst_offset += sizeof(std::string);
            }
        } else {
            std::memcpy(dst_ptr + dst_offset, src_ptr, sizes[i]);

            // Advance pointers
            src_ptr += sizes[i];
            dst_offset += sizes[i];
        }
    }

    // Handle final padding (if any) — just skip, no data to copy
    if (padding_index < paddings.size() && paddings[padding_index].offset == dst_offset) {
        // Optionally: zero out final padding
        // std::memset(dstPtr + dstOffset, 0, paddings[paddingIndex].size);
        // not needed — padding is unused.
    }
}

std::string Schema::get_schema() const {
    std::ostringstream oss;
    std::string type;
        
    oss << '|';
    for (auto& c: columns) {
        switch (c.type)
        {
        case DataType::CHAR: type = "CHAR"; break;
        case DataType::STRING: type = "STRING"; break;
        case DataType::INT8: type = "INT8"; break;
        case DataType::INT16: type = "INT16"; break;
        case DataType::INT32: type = "INT32"; break;
        case DataType::INT64: type = "INT64"; break;
        case DataType::FLOAT32: type = "FLOAT32"; break;
        case DataType::FLOAT64: type = "FLOAT64"; break;
        }
        if (c.count > 1)
            type += "[" + std::to_string(c.count) + "]";
        oss << c.name << ":" << type << "|";
    }

    return oss.str();
}

Table::Table(const std::string& name): name(name),
                                       file_name(name + "_table.db"),
                                       strings_file_name(name + "_table_strings.db") {
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) throw std::runtime_error("Table file does not exist");
    read_metadata();
}

Table::Table(const std::string& name, const Schema& schema): schema(schema),
                                                             name(name),
                                                             file_name(name + "_table.db"),
                                                             strings_file_name(name + "_table_strings.db") {
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        int size = file.tellg();
        if (size == 0) {
            columns = schema.get_columns();
            element_size = schema.get_row_size();
            frame_size = element_size * 64;
            if (frame_size <= MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
            else if (!(frame_size < MAX_FRAME_SIZE)) throw std::runtime_error("Element size too big");
            frame_capacity = frame_size / element_size;
            initialize_file();
        } else {
            read_metadata();
            if (columns != schema.get_columns())
                throw std::runtime_error("Incompatible schema and metadata");
        }
    } else {
        columns = schema.get_columns();
        element_size = schema.get_row_size();
        frame_size = element_size * 64;
        if (frame_size <= MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
        else if (!(frame_size < MAX_FRAME_SIZE)) throw std::runtime_error("Element size too big");
        frame_capacity = frame_size / element_size;
        initialize_file();
    }
}

Table::Table(const std::string& name, const std::vector<column>& columns): schema(columns),
                                                                           name(name),
                                                                           file_name(name + "_table.db"),
                                                                           strings_file_name(name + "_table_strings.db") {
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        int size = file.tellg();
        if (size == 0) {
            this->columns = columns;
            element_size = schema.get_row_size();
            frame_size = element_size * 64;
            if (frame_size <= MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
            else if (!(frame_size < MAX_FRAME_SIZE)) throw std::runtime_error("Element size too big");
            frame_capacity = frame_size / element_size;
            initialize_file();
        } else {
            read_metadata();
            if (columns != schema.get_columns())
                throw std::runtime_error("Incompatible schema and metadata");
        }
    } else {
        Schema schema(columns);
        this->columns = columns;
        element_size = schema.get_row_size();
        frame_size = element_size * 64;
        if (frame_size <= MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
        else if (!(frame_size < MAX_FRAME_SIZE)) throw std::runtime_error("Element size too big");
        frame_capacity = frame_size / element_size;
        initialize_file();
    }
}

Table::~Table() {
    write_metadata();
    flush_all();
    file.close();
}

void Table::initialize_file() {
    file.close();
    fs::remove(file_name);
    file.open(file_name, std::ios::out);
    file.close();
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    write_metadata();
    if (std::any_of(columns.begin(), columns.end(), [](const column& c) {return c.type==DataType::STRING;})) {
        strings_file.close();
        fs::remove(strings_file_name);
        strings_file.open(strings_file_name, std::ios::out);
        strings_file.close();
        strings_file.open(strings_file_name, std::ios::binary | std::ios::in | std::ios::out);
    }
}

void Table::write_metadata() {

    /*
     Metadata format:
        Schema length (4 bytes integer)
        Schema (string) | column1 | column2 | ...
        Frame size (4 bytes integer)
        Frames count (4 bytes integer)
        Elements count (4 bytes integer)
    */

    std::string schema = Schema(columns).get_schema();
    int schema_size = schema.length();
    int frames_count = frames.size();

    if (schema_size + 16 > METADATA_LENGTH) throw std::runtime_error("Metadata too big");

    file.seekp(0, std::ios::beg);

    file.write((char*)&schema_size, 4); // schema length
    file.write(schema.data(), schema_size); // schema
    file.write((char*)&frame_size, 4); // frame size
    file.write((char*)&frames_count, 4); // frames count
    file.write((char*)&elements_count, 4); // elements count
}

void Table::read_metadata() {
    file.seekg(0, std::ios::beg);

    int schema_size;
    // reads the schema size
    file.read((char*)&schema_size, 4);
    if (schema_size <= 0) throw std::runtime_error("Invalid metadata: invalid schema size");
    if (schema_size + 16 > METADATA_LENGTH) throw std::runtime_error("Invalid metadata: metadata too big");

    std::string headers; // the columns names and types
    // reads the schema
    headers.resize(schema_size);
    file.read(headers.data(), schema_size);

    // Make schema to use helper functions
    schema = Schema(headers);

    columns = schema.get_columns();
    element_size = schema.get_row_size();

    // reads frame size
    file.read((char*)&frame_size, 4);
    if (frame_size > MAX_FRAME_SIZE) throw std::runtime_error("Invalid metadata: frame size too big");
    if (frame_size < MIN_FRAME_SIZE) throw std::runtime_error("Invalid metadata: frame size too small");

    // calculate frame capacity
    frame_capacity = frame_size / element_size;
    if (frame_capacity <= 0) throw std::runtime_error("Invalid metadata: frame capacity too small <= 0");

    int frames_count = 0;
    // reads frames count and elements count
    file.read((char*)&frames_count, 4);
    file.read((char*)&elements_count, 4);
    if (frames_count < 0) throw std::runtime_error("Invalid metadata: frames count must be a positive number");

    // Get all existing frames positions and number of elements
    for (int i = 0; i < frames_count; i++) {
        auto f = std::make_unique<frame>();
        f->file_pos = METADATA_LENGTH + i * frame_size + 4;
        file.seekg(f->file_pos - 4, std::ios::beg);
        file.read((char*)&(f->count), 4);
        if (f->count < 0) throw std::runtime_error("Invalid metadata: frame count can not be less than 0");
        frames.push_back(std::move(f));
    }

    if (std::any_of(columns.begin(), columns.end(), [](const column& c) {return c.type == DataType::STRING;})) {
        strings_file.open(strings_file_name, std::ios::binary | std::ios::in | std::ios::out);
        if (!strings_file.is_open()) {
            strings_file.close();
            strings_file.open(strings_file_name, std::ios::out);
            strings_file.close();
            strings_file.open(strings_file_name, std::ios::binary | std::ios::in | std::ios::out);
        }
    }
}

void Table::add_frame() {
    auto f = std::make_unique<frame>();
    f->file_pos = frames.size() * (frame_size + 4) + METADATA_LENGTH + 4;
    file.seekp(f->file_pos - 4, std::ios::beg);

    file.write((char*)&(f->count), 4);

    frames.push_back(std::move(f));
}

void Table::load_frame(frame& f) {
    f.accessed.store(true);
    if (!f.data) {
        // frame lock
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        // data buffer
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(frame_size);
        // file lock
        std::shared_lock<std::shared_mutex> file_lock(file_mutex);

        // file.clear resets the file state
        file.clear(); // this line is to solve the write errors
        // but it could cause errors and bugs if the file was realy damaged

        file.seekg(f.file_pos - 4, std::ios::beg);
        file.read((char*)&(f.count), 4);
        file.read(buffer.get(), frame_size);
        f.data = std::move(buffer);
        //frame* ptr = &f;
        std::thread(&Table::unload_frame, this, std::ref(f)).detach();
    }
}

void Table::unload_frame(frame& f) {
    while ((f.accessed.load())) {
        f.accessed.store(false);
        sleep(CACHE_LT_S);
    }
    if (f.data) {
        flush_frame(f);
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        f.count = 0;
        f.data.reset();
    }
}

void Table::flush_frame(frame& f) {
    if (f.data) {
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        std::unique_lock<std::shared_mutex> file_lock(file_mutex);

        // file.clear() resets the file state
        file.clear(); // this line is to solve the write errors
        // but it could cause errors and bugs if the file was realy damaged

        file.seekp(f.file_pos - 4, std::ios::beg);
        file.write((char*)(&f.count), 4);
        file.write(f.data.get(), frame_size);
        file.flush();
    }
}

void Table::flush_all() {
    for (auto& f: frames)
        flush_frame(*f);
}

void Table::add(void* buffer, int count) {
    if (count <= 0) return;
    for (char* b = static_cast<char*>(buffer); b < static_cast<char*>(buffer) + (element_size * count); b += element_size) {
        bool added = false;
        for (auto& f: frames) {
            if (f->count < frame_capacity) {
                load_frame(*f);
                std::unique_lock<std::shared_mutex> lock(f->mutex);
                for (auto& s: schema.get_strings_offsets()) {
                    *(char**)(b + s + 4) = add_string(*(char**)(b + s + 4), *(int*)(b + s));
                }
                std::memcpy(f->data.get() + (f->count * element_size), b, element_size);
                (f->count)++;
                elements_count++;
                added = true;
                break;
            }
        }
        if (!added) {
            add_frame();
            auto& f = frames[frames.size() - 1];
            load_frame(*f);
            std::unique_lock<std::shared_mutex> lock(f->mutex);
            for (auto& s: schema.get_strings_offsets()) {
                *(char**)(b + s + 4) = add_string(*(char**)(b + s + 4), *(int*)(b + s));
            }
            std::memcpy(f->data.get() + (f->count * element_size), b, element_size);
            (f->count)++;
            elements_count++;
        }
    }
}

void* Table::get_at(int index) {
    if (index > elements_count) return nullptr;
    char* e = new char[element_size];
    int count = 0;
    for (auto& f: frames) {
        load_frame(*f);
        std::shared_lock<std::shared_mutex> lock(f->mutex);
        count += f->count;
        if (index < count) {
            std::memcpy(e, f->data.get() + ((f->count - (count - index)) * element_size), element_size);
            for (auto& s: schema.get_strings_offsets()) {
                char* str = get_string(*(char**)(e + s + 4), *(int*)(e + s));
                //memcpy(e + s + 4, &str, 8);
                *(char**)(e + s + 4) = str;
                //std::cout << (long long)str << std::endl;
            }
            return e;
        }
    }
    delete[] e;
    return nullptr;
}

char* Table::add_string(const char* str, const int len) {
    if (len < 0) throw std::invalid_argument("String length cannot be negative");
    if (len == 0) return nullptr;

    char* pointer;

    std::unique_lock<std::shared_mutex> lock(strings_file_mutex);
    strings_file.clear();
    strings_file.seekg(0, std::ios::beg);

    while (true) {
        uint length = 0;
        strings_file.read((char*)&length, 4);
        if (length == 0) {
            int count = 0;
            while (true) {
                char byte = 0;
                strings_file.read(&byte, 1);
                if (byte != 0 || count >= len) break;
                count++;
            }
            if (count >= len) {
                if (strings_file.tellg() == -1) {
                    strings_file.clear();
                    strings_file.seekg(0, std::ios::end);
                    pointer = (char*)(long long)(strings_file.tellg());
                } else {
                    pointer = (char*)(long long)(strings_file.tellg()) - count;
                }
                break;
            }
        }
        strings_file.seekg(length, std::ios::cur);
    }

    strings_file.clear();
    strings_file.seekp((long long)pointer, std::ios::beg);
    strings_file.write((char*)&len, 4);
    strings_file.write(str, len);
    strings_file.flush();

    return pointer;
}

char* Table::get_string(const char* ptr, const int len) {
    char* result = nullptr;
    if (len == 0) return result;

    std::shared_lock<std::shared_mutex> lock(strings_file_mutex);
    strings_file.clear();
    strings_file.seekg(0, std::ios::end);
    if ((long long)ptr > strings_file.tellg()) throw std::out_of_range("String pointer " + std::to_string((long long)ptr) + " out of file range");
    strings_file.seekg((long long)ptr, std::ios::beg);
    int length = 0;
    strings_file.read((char*)&length, 4);
    if (length != len) throw std::runtime_error("Incompatible string length");
    result = (char*)malloc(length);
    if (result == nullptr) throw std::runtime_error("Memory allocation failed");
    strings_file.read(result, length);

    return result;
}

void Table::remove_string(const char* ptr, const int len) {
    char* result = nullptr;
    if (len == 0) return;

    std::shared_lock<std::shared_mutex> lock1(strings_file_mutex);
    strings_file.clear();
    strings_file.seekg(0, std::ios::end);
    if ((long long)ptr > strings_file.tellg()) throw std::out_of_range("String pointer " + std::to_string((long long)ptr) + " out of file range");
    strings_file.seekg((long long)ptr, std::ios::beg);
    int length = 0;
    strings_file.read((char*)&length, 4);
    if (length != len) throw std::runtime_error("Incompatible string length");
    lock1.unlock();

    std::unique_lock<std::shared_mutex> lock2(strings_file_mutex);
    file.seekp((long long)ptr, std::ios::beg);
    length = 0;
    file.write((char*)&length, sizeof(int));
    for (int i = 0; i < len; i++) {
        file.write((char*)&length, 1);
    }
    file.flush();
}

void Table::clear() {
    for (auto& f: frames) {
        load_frame(*f);
        std::unique_lock<std::shared_mutex> lock(f->mutex);
        elements_count -= f->count;
        f->count = 0;
        std::fill<char*, char>(f->data.get(), f->data.get() + frame_size, 0);
    }
    frames.clear();
    std::unique_lock<std::shared_mutex> lock(file_mutex);
    initialize_file();
}

Table::query_result Table::add(const std::string& e) {
    query_result result;
    if (e.empty()) return result;

    std::vector<std::string> elements;
    char quot = '\0';
    char bracket = '\0';
    const char* beg = nullptr;
    std::string val;

    std::list<std::string> strings;

    for (const char* i = e.data(); i < e.data() + e.length(); i++) {
        if (*i != '(' && *i != ' ' && quot == '\0') {
            break;
        }
        if (*i == '(' && quot == '\0') {
            if (bracket != '\0')
                throw std::invalid_argument("Invalid query: opening brackets before the closing ones");
            bracket = '(';
            beg = i + 1;
        } else if (*i == ')' && quot == '\0') {
            if (bracket != '(')
                throw std::invalid_argument("Invalid query: closing brackets before the opening ones");
            elements.push_back(std::string(beg, i - beg));
            bracket = '\0';
        }
    }

    // If there is no pantheses (typicly means there is only one element which wont be added with the previous loop)
    if (elements.empty()) elements.push_back(e);

    std::vector<std::unique_ptr<char[]>> buffers;
    for (auto& x: elements) {
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(element_size);
        char* ptr = buffer.get();
        const char* last = x.data();

        for (auto& c: columns) {
            if (c.count == 1) {
                val = "";
                long long number = 0;
                float decimal32 = 0;
                double decimal64 = 0;
                bool is_negative = false;
                bool is_decimal = false;
                bool is_set = false;

                switch (c.type) {
                case DataType::INT8:
                case DataType::INT16:
                case DataType::INT32:
                case DataType::INT64:
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == ',') {
                            last = i + 1;
                            break;
                        }
                        else if (*i == '-') {
                            if (is_negative)
                                throw std::invalid_argument("Invalid query: the minus sign appeared twice");
                            is_negative = true;
                            val += *i;
                        } else if ((*i >= '0' && *i <= '9')) {
                            val += *i;
                        } else if (*i != ' ') {
                            throw std::invalid_argument("Invalid query: invalid character for a number: '" + std::string(1, *i) + '\'');
                        }
                    }
                    number = std::stoll(val);

                    // Fixed: Check both overflow and underflow
                    if ((c.type == DataType::INT8 && (number > INT8_MAX || number < INT8_MIN)) ||
                        (c.type == DataType::INT16 && (number > INT16_MAX || number < INT16_MIN)) ||
                        (c.type == DataType::INT32 && (number > INT32_MAX || number < INT32_MIN)) ||
                        (c.type == DataType::INT64 && (number > INT64_MAX || number < INT64_MIN)))
                        throw std::invalid_argument("Invalid query: number out of range for its type");

                    if (c.type == DataType::INT8) *(ptr++) = (char)number;
                    else if (c.type == DataType::INT16) {
                        *(short*)ptr = (short)number;
                        ptr += 2;
                    }
                    else if (c.type == DataType::INT32) {
                        *(int*)ptr = (int)number;
                        ptr += 4;
                    }
                    else if (c.type == DataType::INT64) {
                        *(long long*)ptr = (long long)number;
                        ptr += 8;
                    }
                    break;

                case DataType::FLOAT32:
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == ',') {
                            last = i + 1;
                            break;
                        }
                        else if (*i == '-') {
                            if (is_negative)
                                throw std::invalid_argument("Invalid query: the minus sign appeared twice");
                            is_negative = true;
                            val += *i;
                        } else if ((*i >= '0' && (*i <= '9'))) {
                            val += *i;
                        } else if (*i == '.') {
                            if (is_decimal)
                                throw std::invalid_argument("Invalid query: decimal point twice in one number");
                            is_decimal = true;
                            val += *i;  // Fixed: Need to add decimal point to val
                        } else if (*i != ' ') {
                            throw std::invalid_argument("Invalid query: invalid character for a number");
                        }
                    }
                    decimal32 = std::stof(val);
                    *(float*)ptr = decimal32;
                    ptr += 4;
                    break;

                case DataType::FLOAT64:
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == ',') {
                            last = i + 1;
                            break;
                        }
                        else if (*i == '-') {
                            if (is_negative)
                                throw std::invalid_argument("Invalid query: the minus sign appeared twice");
                            is_negative = true;
                            val += *i;
                        } else if ((*i >= '0' && (*i <= '9'))) {
                            val += *i;
                        } else if (*i == '.') {
                            if (is_decimal)
                                throw std::invalid_argument("Invalid query: decimal point twice in one number");
                            is_decimal = true;
                            val += *i;  // Fixed: Need to add decimal point to val
                        } else if (*i != ' ') {
                            throw std::invalid_argument("Invalid query: invalid character for a number");
                        }
                    }
                    decimal64 = std::stod(val);
                    *(double*)ptr = decimal64;
                    ptr += 8;
                    break;

                case DataType::CHAR:
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == ',' && is_set) {
                            last = i + 1;
                            break;
                        } else if (*i == '\'' || *i == '"') {  // Fixed: Changed && to ||
                            if ((i + 2 < x.data() + x.length() && *(i + 2) != *i) || is_set)
                                throw std::invalid_argument("Invalid query: invalid value for type CHAR");
                            *ptr = *(i + 1);
                            ptr++;
                            is_set = true;
                        } else if (*i != ' ') {
                            throw std::invalid_argument("Invalid query: invalid value for type CHAR");
                        }
                    }
                    break;

                case DataType::STRING:
                {
                    const char* beg = nullptr;
                    const char* end = nullptr;
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == '\'' || *i == '"') {
                            if (beg != nullptr && *(beg - 1) == *i) {
                                end = i;
                            } else if (beg == nullptr) {
                                beg = i + 1;
                            }
                        } else if (*i == ',' && end != nullptr) {
                            last = i + 1;
                            break;
                        }
                    }
                    if (beg == nullptr || end == nullptr)
                        throw std::invalid_argument("Invalid query: unterminated string");
                    int length = end - beg;
                    strings.push_back(std::string(beg, length));
                    *(int*)ptr = length;
                    ptr += 4;
                    *(char**)ptr = strings.back().data();
                    ptr += 8;
                    break;
                }

                default:
                    throw std::runtime_error("Unsupported type");
                    break;
                }
            } else {
                if (c.count <= 0) throw std::runtime_error("Negative / zero size array");
                val = "";
                const char* beg = nullptr;
                const char* end = nullptr;

                switch (c.type) {
                case DataType::INT8:
                case DataType::INT16:
                case DataType::INT32:
                case DataType::INT64:
                case DataType::FLOAT32:
                case DataType::FLOAT64:
                {
                    char array_open = '\0';
                    char array_close = '\0';
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == '{' || *i == '[') {
                            array_open = *i;
                            array_close = (*i == '{') ? '}' : ']';
                            beg = i + 1;
                            break;
                        } else if (*i != ' ') {
                            throw std::invalid_argument("Invalid query: expected array opening bracket");
                        }
                    }

                    if (array_open == '\0')
                        throw std::invalid_argument("Invalid query: array opening bracket not found");

                    // find closing bracket
                    for (const char* i = beg; i < x.data() + x.length(); i++) {
                        if (*i == array_close) {
                            end = i;
                            break;
                        }
                    }

                    if (end == nullptr)
                        throw std::invalid_argument("Invalid query: array closing bracket not found");

                    std::vector<std::string> array_vals;
                    std::string current_val;
                    for (const char* i = beg; i < end; i++) {
                        if (*i == ',') {
                            if (!current_val.empty()) {
                                array_vals.push_back(current_val);
                                current_val.clear();
                            }
                        } else if (*i != ' ') {
                            current_val += *i;
                        }
                    }
                    if (!current_val.empty()) {
                        array_vals.push_back(current_val);
                    }

                    if (array_vals.size() > c.count)
                        throw std::invalid_argument("Invalid query: too many elements in array");

                    // Convert and store values
                    for (int idx = 0; idx < array_vals.size(); idx++) {
                        if (c.type == DataType::INT8 || c.type == DataType::INT16 ||
                            c.type == DataType::INT32 || c.type == DataType::INT64) {
                            long long num = std::stoll(array_vals[idx]);

                            if ((c.type == DataType::INT8 && (num > INT8_MAX || num < INT8_MIN)) ||
                                (c.type == DataType::INT16 && (num > INT16_MAX || num < INT16_MIN)) ||
                                (c.type == DataType::INT32 && (num > INT32_MAX || num < INT32_MIN)) ||
                                (c.type == DataType::INT64 && (num > INT64_MAX || num < INT64_MIN)))
                                throw std::invalid_argument("Invalid query: array element out of range");

                            if (c.type == DataType::INT8) {
                                *ptr = (char)num;
                                ptr++;
                            } else if (c.type == DataType::INT16) {
                                *(short*)ptr = (short)num;
                                ptr += 2;
                            } else if (c.type == DataType::INT32) {
                                *(int*)ptr = (int)num;
                                ptr += 4;
                            } else if (c.type == DataType::INT64) {
                                *(long long*)ptr = (long long)num;
                                ptr += 8;
                            }
                        } else if (c.type == DataType::FLOAT32) {
                            float fval = std::stof(array_vals[idx]);
                            *(float*)ptr = fval;
                            ptr += 4;
                        } else if (c.type == DataType::FLOAT64) {
                            double dval = std::stod(array_vals[idx]);
                            *(double*)ptr = dval;
                            ptr += 8;
                        }
                    }

                    // fill remaining space with zeros
                    int element_type_size = (c.type == DataType::INT8) ? 1 :
                                                   (c.type == DataType::INT16) ? 2 :
                                                   (c.type == DataType::INT32 || c.type == DataType::FLOAT32) ? 4 : 8;
                    int remaining = c.count - array_vals.size();
                    std::memset(ptr, 0, remaining * element_type_size);
                    ptr += remaining * element_type_size;

                    for (const char* i = end + 1; i < x.data() + x.length(); i++) {
                        if (*i == ',') {
                            last = i + 1;
                            break;
                        }
                    }
                    break;
                }
                case DataType::CHAR:
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if (*i == '\'' || *i == '"') {
                            if (beg != nullptr && *(beg - 1) == *i) {
                                end = i;
                            } else if (beg == nullptr) {
                                beg = i + 1;
                            }
                        } else if (*i == ',' && end != nullptr) {
                            last = i + 1;
                            break;
                        }
                    }
                    if (beg == nullptr || end == nullptr)
                        throw std::invalid_argument("Invalid query: unterminated string");
                    if (end - beg > c.count)
                        throw std::invalid_argument("Invalid query: too long string");
                    std::memcpy(ptr, beg, end - beg);
                    // Fill remaining space with zeros
                    std::memset(ptr + (end - beg), 0, c.count - (end - beg));
                    ptr += c.count;
                    break;

                case DataType::STRING:
                {
                    quot = '\0';
                    char array_open = '\0';
                    char array_close = '\0';
                    for (const char* i = last; i < x.data() + x.length(); i++) {
                        if ((*i == '{' || *i == '[') && quot == '\0') {
                            array_open = *i;
                            array_close = (*i == '{') ? '}' : ']';
                            beg = i + 1;
                            break;
                        } else if (*i != ' ') {
                            throw std::invalid_argument("Invalid query: expected array opening bracket");
                        }
                    }

                    if (array_open == '\0')
                        throw std::invalid_argument("Invalid query: array opening bracket not found");

                    // find closing bracket
                    for (const char* i = beg; i < x.data() + x.length(); i++) {
                        if (*i == array_close) {
                            end = i;
                            break;
                        }
                    }

                    if (end == nullptr)
                        throw std::invalid_argument("Invalid query: array closing bracket not found");

                    std::vector<std::string> array_vals;
                    std::string current_val;
                    quot = '\0';
                    for (const char* i = beg; i < end; i++) {
                        if (*i == ',' && quot == '\0') {
                            continue;
                        } else if ((*i == '\'' || *i == '"')) {
                            if (quot == '\0') quot = *i;
                            else if (quot == *i) {
                                quot = '\0';
                                array_vals.push_back(current_val);
                                current_val.clear();
                            }
                        } else if (*i != ' ' && quot != '\0') {
                            current_val += *i;
                        }
                    }
                    if (!current_val.empty()) {
                        array_vals.push_back(current_val);
                    }

                    if (array_vals.size() > c.count)
                        throw std::invalid_argument("Invalid query: too many elements in array");

                    // Convert and store values
                    for (auto& s: array_vals) {
                        strings.push_back(s);
                        *(int*)ptr = s.length();
                        ptr += 4;
                        *(char**)ptr = strings.back().data();
                        ptr += 8;
                    }

                    // fill remaining space with zeros
                    int element_type_size = 12;
                    int remaining = c.count - array_vals.size();
                    std::memset(ptr, 0, remaining * element_type_size);
                    ptr += remaining * element_type_size;

                    for (const char* i = end + 1; i < x.data() + x.length(); i++) {
                        if (*i == ',') {
                            last = i + 1;
                            break;
                        }
                    }
                    break;
                }

                default:
                    throw std::runtime_error("Unsupported type");
                    break;
                }
            }
        }
        buffers.push_back(std::move(buffer));
    }

    for (auto& b: buffers) {
        add(b.get());
        result.rows_added++;
    }
    result.rows_affected = result.rows_added;

    return result;
}
