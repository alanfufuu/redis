#include "persistence.h"
#include <fstream>
#include <iostream>
#include <filesystem>

static const char* MAGIC = "RDCLONE1";
static const char* FOOTER = "EOF";

Persistence::Persistence(const std::string& filepath) : filepath_(filepath) {}

void Persistence::writeUint8(std::ofstream& out, uint8_t val) {
    out.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

void Persistence::writeUint32(std::ofstream& out, uint32_t val) {
    out.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

void Persistence::writeDouble(std::ofstream& out, double val) {
    out.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

void Persistence::writeString(std::ofstream& out, const std::string& str) {
    writeUint32(out, static_cast<uint32_t>(str.size()));
    out.write(str.data(), str.size());
}

uint8_t Persistence::readUint8(std::ifstream& in) {
    uint8_t val;
    in.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

uint32_t Persistence::readUint32(std::ifstream& in) {
    uint32_t val;
    in.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

double Persistence::readDouble(std::ifstream& in) {
    double val;
    in.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

std::string Persistence::readString(std::ifstream& in) {
    uint32_t len = readUint32(in);
    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}


bool Persistence::save(const std::unordered_map<std::string, StoreValue>& data) {
    std::string temp_path = filepath_ + ".tmp";

    std::ofstream out(temp_path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Persistence: failed to open " << temp_path << std::endl;
        return false;
    }

    out.write(MAGIC, 8);

    writeUint32(out, static_cast<uint32_t>(data.size()));

    for (const auto& [key, value] : data) {
        if (std::holds_alternative<std::string>(value)) {
            writeUint8(out, 0);
            writeString(out, key);
            writeString(out, std::get<std::string>(value));

        } else if (std::holds_alternative<std::list<std::string>>(value)) {
            writeUint8(out, 1);
            writeString(out, key);

            const auto& lst = std::get<std::list<std::string>>(value);
            writeUint32(out, static_cast<uint32_t>(lst.size()));
            for (const auto& elem : lst) {
                writeString(out, elem);
            }

        } else if (std::holds_alternative<SortedSet>(value)) {
            writeUint8(out, 2);
            writeString(out, key);

            const auto& zset = std::get<SortedSet>(value);
            auto members = zset.range(0, static_cast<int64_t>(zset.size()) - 1, true);
            uint32_t count = static_cast<uint32_t>(members.size() / 2);
            writeUint32(out, count);

            for (size_t i = 0; i < members.size(); i += 2) {
                double score = std::stod(members[i + 1]);
                writeDouble(out, score);
                writeString(out, members[i]);
            }
        }
    }

    out.write(FOOTER, 3);
    out.close();


    std::filesystem::rename(temp_path, filepath_);

    std::cout << "Persistence: saved " << data.size() << " keys to "
              << filepath_ << std::endl;

    return true;
}


bool Persistence::load(Store& store) {
    std::ifstream in(filepath_, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    char magic[8];
    in.read(magic, 8);
    if (std::string(magic, 8) != MAGIC) {
        std::cerr << "Persistence: invalid file header" << std::endl;
        return false;
    }

    uint32_t num_keys = readUint32(in);
    std::cout << "Persistence: loading " << num_keys << " keys from "
              << filepath_ << std::endl;

    for (uint32_t i = 0; i < num_keys; i++) {
        if (!in.good()) {
            std::cerr << "Persistence: unexpected end of file" << std::endl;
            return false;
        }

        uint8_t type = readUint8(in);
        std::string key = readString(in);

        if (type == 0) {
            std::string value = readString(in);
            store.set(key, value);

        } else if (type == 1) {
            uint32_t count = readUint32(in);
            std::vector<std::string> elements;
            for (uint32_t j = 0; j < count; j++) {
                elements.push_back(readString(in));
            }
            store.rpush(key, elements);

        } else if (type == 2) {
            uint32_t count = readUint32(in);
            std::vector<std::pair<double, std::string>> entries;
            for (uint32_t j = 0; j < count; j++) {
                double score = readDouble(in);
                std::string member = readString(in);
                entries.emplace_back(score, member);
            }
            store.zadd(key, entries);

        } else {
            std::cerr << "Persistence: unknown type " << (int)type << std::endl;
            return false;
        }
    }

    // Verify footer
    char footer[3];
    in.read(footer, 3);
    if (std::string(footer, 3) != FOOTER) {
        std::cerr << "Persistence: missing footer — file may be corrupt" << std::endl;
        return false;
    }

    std::cout << "Persistence: load complete" << std::endl;
    return true;
}

bool Persistence::fileExists() const {
    return std::filesystem::exists(filepath_);
}