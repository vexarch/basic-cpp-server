#include "vx_database.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

Schema::Schema() {}

Schema::Schema(const std::string& schema) {
    // | column1 | column2 | column3[123] | ...
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

        if (type == "CHAR") c.type = DataType::CHAR;
        else if (type == "WCHAR") c.type = DataType::WCHAR;
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
}

Schema::Schema(const std::vector<column>& columns): columns(columns) {}

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
}

int Schema::get_row_size() const {
    int size = 0;
    for (auto& c: columns) {
        int x = 1;
        switch (c.type) {
        case DataType::WCHAR:
        case DataType::INT16:
            x = 2;
            break;
        case DataType::INT32:
        case DataType::FLOAT32:
            x = 4;
            break;
        case DataType::INT64:
        case DataType::FLOAT64:
            x = 8;
            break;
        }
        size += c.count * x;
    }
    return size;
}

std::vector<int> Schema::get_sizes() const {
    std::vector<int> sizes;
    for (auto& c: columns) {
        int x = 1;
        switch (c.type) {
        case DataType::WCHAR:
        case DataType::INT16:
            x = 2;
            break;
        case DataType::INT32:
        case DataType::FLOAT32:
            x = 4;
            break;
        case DataType::INT64:
        case DataType::FLOAT64:
            x = 8;
            break;
        }
        sizes.push_back(x * c.count);
    }
    return sizes;
}

std::vector<column> Schema::get_columns() const {
    return columns;
}

std::string Schema::get_schema() const {
    std::ostringstream oss;
    std::string type;
        
    oss << '|';
    for (auto& c: columns) {
        switch (c.type)
        {
        case DataType::CHAR: type = "CHAR"; break;
        case DataType::WCHAR: type = "WCHAR"; break;
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

Table::Table(const std::string& name): name(name), file_name(name + "_table.db") {
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) throw std::runtime_error("Table file does not exist");
    read_metadata();
}

Table::Table(const std::string& name, const Schema& schema): name(name), file_name(name + "_table.db") {
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        int size = file.tellg();
        if (size == 0) {
            columns = schema.get_columns();
            element_size = schema.get_row_size();
            sizes = schema.get_sizes();
            paddings = calculate_struct_padding(sizes);
            frame_size = element_size * 64;
            if (frame_size < MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
            else if (element_size < MAX_FRAME_SIZE) frame_size = MAX_FRAME_SIZE;
            else throw std::runtime_error("Element size too big");
            frame_capacity = frame_size / element_size;
            initialize_file();
        } else {
            read_metadata();
            auto tmp = schema.get_columns();
            if (columns.size() != tmp.size()) throw std::runtime_error("Incompatible schema and metadata");
            for (int i = 0; i < columns.size(); i++)
                if (columns[i].name != tmp[i].name || columns[i].type != tmp[i].type || columns[i].count != tmp[i].count)
                    throw std::runtime_error("Incompatible schema and metadata");
        }
    } else {
        columns = schema.get_columns();
        element_size = schema.get_row_size();
        sizes = schema.get_sizes();
        paddings = calculate_struct_padding(sizes);
        frame_size = element_size * 64;
        if (frame_size < MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
        else if (element_size < MAX_FRAME_SIZE) frame_size = MAX_FRAME_SIZE;
        else throw std::runtime_error("Element size too big");
        frame_capacity = frame_size / element_size;
        initialize_file();
    }
}

Table::Table(const std::string& name, const std::vector<column>& columns): name(name), file_name(name + "_table.db") {
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        int size = file.tellg();
        if (size == 0) {
            Schema schema(columns);
            this->columns = columns;
            element_size = schema.get_row_size();
            sizes = schema.get_sizes();
            paddings = calculate_struct_padding(sizes);
            frame_size = element_size * 64;
            if (frame_size < MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
            else if (element_size < MAX_FRAME_SIZE) frame_size = MAX_FRAME_SIZE;
            else throw std::runtime_error("Element size too big");
            frame_capacity = frame_size / element_size;
            initialize_file();
        } else {
            read_metadata();
            if (this->columns.size() != columns.size()) throw std::runtime_error("Incompatible schema and metadata");
            for (int i = 0; i < columns.size(); i++)
                if (this->columns[i].name != columns[i].name ||
                    this->columns[i].type != columns[i].type ||
                    this->columns[i].count != columns[i].count)
                    throw std::runtime_error("Incompatible schema and metadata");
        }
    } else {
        Schema schema(columns);
        this->columns = columns;
        element_size = schema.get_row_size();
        sizes = schema.get_sizes();
        paddings = calculate_struct_padding(sizes);
        frame_size = element_size * 64;
        if (frame_size < MIN_FRAME_SIZE) frame_size = MIN_FRAME_SIZE;
        else if (element_size < MAX_FRAME_SIZE) frame_size = MAX_FRAME_SIZE;
        else throw std::runtime_error("Element size too big");
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
    file.open(file_name, std::ios::out);
    file.close();
    file.open(file_name, std::ios::binary | std::ios::in | std::ios::out);
    write_metadata();
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
    //int frame_size = frame_capacity;
    int frames_count = frames.size();
    /*int elements_count = 0;
    for (auto& f: frames) {
        std::shared_lock<std::shared_mutex> lock(f->mutex);
        elements_count += f->count;
    }*/
    if (schema_size + 16 > METADATA_LENGTH) throw std::runtime_error("Metadata too big");
    file.seekp(0, std::ios::beg);
    file.write((char*)&schema_size, 4);
    file.write(schema.data(), schema_size);
    file.write((char*)&frame_size, 4);
    file.write((char*)&frames_count, 4);
    file.write((char*)&elements_count, 4);
    int pad = 0;
    if ((pad = METADATA_LENGTH - (schema_size + 16)) > 0) {
        char* b = new char[pad];
        for (char* p = b; p < b + pad; p++) *p = 0;
        file.write(b, pad);
        delete[] b;
    }
}

void Table::read_metadata() {
    file.seekg(0, std::ios::beg);
    int schema_size;
    file.read((char*)&schema_size, 4);
    if (schema_size == 0 || schema_size + 16 > METADATA_LENGTH) throw std::runtime_error("Invalid metadata: metadata too big");

    std::string headers; // the columns names and types
    headers.resize(schema_size);
    file.read(headers.data(), schema_size);
    // Make schema to use helper functions
    Schema schema(headers);
    columns = schema.get_columns();
    element_size = schema.get_row_size();
    sizes = schema.get_sizes();
    paddings = calculate_struct_padding(sizes);

    file.read((char*)&frame_size, 4);
    if (frame_size > MAX_FRAME_SIZE) throw std::runtime_error("Invalid metadata: frame size too big");
    if (frame_size < MIN_FRAME_SIZE) throw std::runtime_error("Invalid metadata: frame size too small");

    frame_capacity = frame_size / element_size;
    if (frame_capacity <= 0) throw std::runtime_error("Invalid metadata: frame capacity too small <= 0");

    int frames_count = 0;
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
}

Table::frame& Table::add_frame() {
    auto f = std::make_unique<frame>();
    f->file_pos = frames.size() * (frame_size + 4) + METADATA_LENGTH + 4;
    file.seekp(f->file_pos, std::ios::beg);
    char* b = new char[frame_size + 4];
    *(int*)b = 0;
    file.write(b, frame_size + 4);
    delete[] b;
    frames.push_back(std::move(f));
    return *frames[frames.size() - 1];
}

void Table::load_frame(frame& f) {
    if (!f.data) {
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(frame_size);
        std::shared_lock<std::shared_mutex> file_lock(file_mutex);
        file.seekg(f.file_pos - 4, std::ios::beg);
        file.read((char*)&(f.count), 4);
        file.read(buffer.get(), frame_size);
        f.data = std::move(buffer);
        frame* ptr = &f;
        std::thread([this, ptr]() {
            while (!(ptr->accessed.load())) {
                ptr->accessed.store(false);
                sleep(CACHE_LT_S);
            }
            this->unload_frame((*ptr));
        }).detach();
    }
}

void Table::unload_frame(frame& f) {
    if (f.data) {
        flush_frame(f);
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        f.data.reset();
    }
}

void Table::flush_frame(frame& f) {
    if (f.data) {
        std::unique_lock<std::shared_mutex> lock(f.mutex);
        std::unique_lock<std::shared_mutex> file_lock(file_mutex);
        file.seekp(f.file_pos - 4, std::ios::beg);
        file.write((char*)&(f.count), 4);
        file.write(f.data.get(), frame_size);
    }
}

void Table::flush_all() {
    for (auto& f: frames)
        flush_frame(*f);
}

void Table::clear() {
    for (auto& f: frames) {
        std::unique_lock<std::shared_mutex> lock(f->mutex);
        load_frame(*f);
        elements_count -= f->count;
        f->count = 0;
        std::fill<char*, char>(f->data.get(), f->data.get() + frame_size, 0);
    }
    std::unique_lock<std::shared_mutex> lock(file_mutex);
    file.close();
    fs::remove(file_name);
    initialize_file();
    frames.clear();
    flush_all();
}
