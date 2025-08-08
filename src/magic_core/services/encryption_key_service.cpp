#include "magic_core/services/encryption_key_service.hpp"

#if defined(__APPLE__)
    #include <Security/Security.h>
    #include <CoreFoundation/CoreFoundation.h>
#endif

namespace magic_core {

// Define the constants from the header
const char* EncryptionKeyService::SERVICE_NAME = "com.magicfolder.database_key";
const char* EncryptionKeyService::ACCOUNT_NAME = "default_user";

std::string EncryptionKeyService::get_database_key() {
    // The top-level function remains the same, calling the platform-specific helpers.
    try {
        std::string key = retrieve_key_from_os();
        if (!key.empty()) {
            return key;
        }

        std::string new_key = generate_new_key();
        save_key_to_os(new_key);
        return new_key;

    } catch (const std::exception& e) {
        throw KeyServiceError("Failed to get or create database key: " + std::string(e.what()));
    }
}

// PLATFORM-SPECIFIC IMPLEMENTATIONS

#if defined(__APPLE__)

template<typename T>
void cf_release(T& cf_object) {
    if (cf_object) {
        CFRelease(cf_object);
        cf_object = nullptr;
    }
}

std::string EncryptionKeyService::generate_new_key() {
    std::string key(32, '\0');
    if (SecRandomCopyBytes(kSecRandomDefault, key.size(), reinterpret_cast<uint8_t*>(&key[0])) != errSecSuccess) {
        throw std::runtime_error("Failed to generate random bytes for key using SecRandomCopyBytes.");
    }
    return key;
}

void EncryptionKeyService::save_key_to_os(const std::string& key) {
    CFStringRef service_ref = CFStringCreateWithCString(NULL, SERVICE_NAME, kCFStringEncodingUTF8);
    CFStringRef account_ref = CFStringCreateWithCString(NULL, ACCOUNT_NAME, kCFStringEncodingUTF8);
    CFDataRef key_ref = CFDataCreate(NULL, (const UInt8*)key.data(), key.size());

    // Delete existing item first to ensure a clean write
    const void* del_keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
    const void* del_values[] = { kSecClassGenericPassword, service_ref, account_ref };
    CFDictionaryRef del_query = CFDictionaryCreate(NULL, del_keys, del_values, 3, NULL, NULL);
    SecItemDelete(del_query); // We don't check for error here, as it's fine if it doesn't exist
    cf_release(del_query);

    // Add new item
    const void* add_keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData };
    const void* add_values[] = { kSecClassGenericPassword, service_ref, account_ref, key_ref };
    CFDictionaryRef add_query = CFDictionaryCreate(NULL, add_keys, add_values, 4, NULL, NULL);
    OSStatus status = SecItemAdd(add_query, NULL);
    
    cf_release(add_query);
    cf_release(service_ref);
    cf_release(account_ref);
    cf_release(key_ref);

    if (status != errSecSuccess) {
        // A more detailed error could be fetched here if needed
        throw std::runtime_error("SecItemAdd failed to save key to keychain.");
    }
}

std::string EncryptionKeyService::retrieve_key_from_os() {
    CFStringRef service_ref = CFStringCreateWithCString(NULL, SERVICE_NAME, kCFStringEncodingUTF8);
    CFStringRef account_ref = CFStringCreateWithCString(NULL, ACCOUNT_NAME, kCFStringEncodingUTF8);

    const void* keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData };
    const void* values[] = { kSecClassGenericPassword, service_ref, account_ref, kCFBooleanTrue };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4, NULL, NULL);

    CFDataRef result_data = NULL;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&result_data);

    cf_release(query);
    cf_release(service_ref);
    cf_release(account_ref);

    if (status == errSecItemNotFound) {
        cf_release(result_data);
        return ""; // Key does not exist, this is not an error.
    }
    if (status != errSecSuccess) {
        cf_release(result_data);
        throw std::runtime_error("SecItemCopyMatching failed to retrieve key from keychain.");
    }

    std::string key(CFDataGetLength(result_data), '\0');
    CFDataGetBytes(result_data, CFRangeMake(0, key.size()), (UInt8*)key.data());
    cf_release(result_data);
    return key;
}

#else // All other platforms (Windows, Linux, etc.)

// On any other platform, these functions will throw an exception,
// preventing the application from running in an insecure state.

std::string EncryptionKeyService::generate_new_key() {
    throw std::logic_error("Secure key generation is only implemented for macOS.");
}

void EncryptionKeyService::save_key_to_os(const std::string& key) {
    throw std::logic_error("Secure key storage is only implemented for macOS.");
}

std::string EncryptionKeyService::retrieve_key_from_os() {
    throw std::logic_error("Secure key retrieval is only implemented for macOS.");
}

#endif

} // namespace magic_core