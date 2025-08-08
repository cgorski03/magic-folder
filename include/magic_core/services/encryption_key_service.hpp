#pragma once

#include <string>
#include <stdexcept>

namespace magic_core {

/**
 * @brief A custom exception for key service errors.
 */
class KeyServiceError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Manages the storage and retrieval of the database encryption key
 *        using the native OS secure credential store.
 */
class EncryptionKeyService {
public:
    /**
     * @brief Gets the database encryption key.
     *
     * If the key already exists in the OS keychain/keystore, it is retrieved.
     * If not, a new cryptographically secure key is generated, stored for
     * future use, and then returned.
     *
     * @return A string containing the raw 256-bit (32-byte) encryption key.
     * @throws KeyServiceError if the key cannot be retrieved or stored.
     */
    static std::string get_database_key();

private:
    static const char* SERVICE_NAME;
    static const char* ACCOUNT_NAME;

    /**
     * @brief Platform-specific implementation to retrieve the key.
     * @return The key as a string, or an empty string if not found.
     */
    static std::string retrieve_key_from_os();

    /**
     * @brief Platform-specific implementation to save the key.
     * @param key The key to save.
     */
    static void save_key_to_os(const std::string& key);

    /**
     * @brief Generates a new, cryptographically secure 256-bit key.
     * @return The new key as a 32-byte string.
     */
    static std::string generate_new_key();
};

}