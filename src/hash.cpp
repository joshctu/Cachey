#include "hash.hpp"
#include "xxhash.h"

Hash_Table::Hash_Table() {}

uint64_t Hash_Table::generateHash(const std::string &key) {
  return XXH64(key.data(), key.size(), HASH_SEED);
}

/**
 * @brief Create a new entry in the index map
 *
 * @param key The key to be hashed and generated
 * @param offset The value to be generated. The byte offset | log #  in the WAL
 *
 */
void Hash_Table::createIndex(const std::string &key, const uint64_t offset) {
  uint64_t key_hash = generateHash(key);
  lookup_table[key_hash] = offset;
};

/**
 * @brief Update an entry in the index map to point to a new entry
 *
 * @param hash The hash for the existing entry to be modified
 */
void Hash_Table::updateIndex(const uint64_t hash, const uint64_t offset) {
  lookup_table[hash] = offset;
}

/**
 * @brief Checks for and returns the byte offset | log # for a given key;
 *
 * @param key The key to index from
 *
 * @return The hash if an entry was found
 * @return Nullopt if no entry found
 */
std::optional<uint64_t> Hash_Table::lookupHash(const std::string &key) {
  uint64_t key_hash = generateHash(key);
  if (lookup_table.contains(key_hash))
    return lookup_table[key_hash];

  return std::nullopt;
}
