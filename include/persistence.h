#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "store.h"
#include <string>

class Persistence {
public:
    explicit Persistence(const std::string& filepath);

    // Save a snapshot of data to disk.
    // Takes an already-copied map (no lock needed).
    bool save(const std::unordered_map<std::string, StoreValue>& data);

    // Load a snapshot from disk into the store.
    bool load(Store& store);

    // Check if a snapshot file exists
    bool fileExists() const;

private:
    std::string filepath_;

    // Binary writing helpers
    void writeUint8(std::ofstream& out, uint8_t val);
    void writeUint32(std::ofstream& out, uint32_t val);
    void writeDouble(std::ofstream& out, double val);
    void writeString(std::ofstream& out, const std::string& str);

    // Binary reading helpers
    uint8_t readUint8(std::ifstream& in);
    uint32_t readUint32(std::ifstream& in);
    double readDouble(std::ifstream& in);
    std::string readString(std::ifstream& in);
};

#endif