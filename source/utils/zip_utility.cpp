#include <iostream>
#include <string>
#include <vector>

// Minizip-ng headers (Assumes they are located in include/minizip-ng)
extern "C" {
    #include "mz.h"
    #include "unzip.h"
}

/**
 * @brief Utility class to handle reading files from a Scratch .sb3 archive (which is a ZIP file)
 * using the Minizip-ng library, designed for cross-platform use.
 */
class ScratchZipReader {
public:
    ScratchZipReader() : zip_handle_(nullptr) {}
    
    // Virtual destructor is important for proper cleanup if this class is inherited.
    virtual ~ScratchZipReader() { closeArchive(); }

    /**
     * @brief Opens the Scratch archive file.
     * @param filepath The path to the .sb3 file (e.g., "D:\my_project.sb3" on Xbox).
     * @return true on success, false on failure.
     */
    bool openArchive(const std::string& filepath) {
        closeArchive(); // Close any existing archive first

        // mz_zip_open is the standard Minizip-ng function for opening archives.
        zip_handle_ = mz_zip_open(filepath.c_str(), 0);

        if (zip_handle_ == nullptr) {
            std::cerr << "Error: Could not open ZIP archive: " << filepath << std::endl;
            return false;
        }

        std::cout << "Successfully opened archive: " << filepath << std::endl;
        return true;
    }

    /**
     * @brief Reads a specific file entry from the opened archive into a string buffer.
     * This is used primarily for JSON files like "project.json".
     * @param entryName The name of the file inside the ZIP (e.g., "project.json").
     * @param outputBuffer Reference to a string to hold the content.
     * @return true on success, false on entry not found or read error.
     */
    bool readEntryToString(const std::string& entryName, std::string& outputBuffer) {
        if (zip_handle_ == nullptr) {
            std::cerr << "Error: Archive is not open. Call openArchive() first." << std::endl;
            return false;
        }

        // 1. Locate the file entry within the archive
        int32_t result = mz_zip_goto_entry(zip_handle_, entryName.c_str());
        if (result != MZ_OK) {
            std::cerr << "Error: Entry not found or failed to seek: " << entryName << std::endl;
            return false;
        }

        // 2. Open the entry for reading
        result = mz_zip_entry_read_open(zip_handle_, 0, nullptr);
        if (result != MZ_OK) {
            std::cerr << "Error: Failed to open entry for reading: " << entryName << std::endl;
            // No need to close entry if open failed, but ensures a clean state:
            // mz_zip_entry_close(zip_handle_); 
            return false;
        }

        // 3. Get the size of the uncompressed data
        mz_zip_entry* entry_info = nullptr;
        result = mz_zip_entry_get_info(zip_handle_, &entry_info);
        if (result != MZ_OK || entry_info == nullptr) {
            std::cerr << "Error: Failed to get entry info." << std::endl;
            mz_zip_entry_close(zip_handle_);
            return false;
        }

        // Safety check: ensure size is positive and manageable for embedded environments
        const int64_t uncompressed_size = entry_info->uncompressed_size;
        if (uncompressed_size <= 0 || uncompressed_size > (50 * 1024 * 1024)) { 
            // 50MB limit is generous for project.json, but necessary for asset files
            std::cerr << "Error: Entry size too large or invalid for embedded system: " << uncompressed_size << " bytes." << std::endl;
            mz_zip_entry_close(zip_handle_);
            return false;
        }

        // 4. Read the data into the string buffer
        try {
            outputBuffer.resize(static_cast<size_t>(uncompressed_size));
        } catch (const std::bad_alloc& e) {
            std::cerr << "Error: Failed to allocate memory for entry " << entryName << ": " << e.what() << std::endl;
            mz_zip_entry_close(zip_handle_);
            return false;
        }

        int32_t bytes_read = mz_zip_entry_read(zip_handle_, outputBuffer.data(), outputBuffer.size());

        // 5. Cleanup
        mz_zip_entry_close(zip_handle_);

        if (bytes_read != uncompressed_size) {
            std::cerr << "Error: Mismatch in bytes read. Expected " << uncompressed_size
                      << ", got " << bytes_read << ". Read operation may have failed or been incomplete." << std::endl;
            return false;
        }

        std::cout << "Successfully read entry: " << entryName << " (" << bytes_read << " bytes)" << std::endl;
        return true;
    }

    /**
     * @brief Closes the open archive handle.
     */
    void closeArchive() {
        if (zip_handle_ != nullptr) {
            mz_zip_close(zip_handle_);
            zip_handle_ = nullptr;
            std::cout << "Archive closed." << std::endl;
        }
    }

private:
    void* zip_handle_; // Opaque pointer used by minizip-ng API
};