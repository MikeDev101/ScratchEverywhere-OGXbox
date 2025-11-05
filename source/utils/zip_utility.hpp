#pragma once

#include <string>
#include <vector>

// Forward declaration of the opaque minizip-ng type
struct mz_zip_entry;
struct mz_zip_file;

/**
 * @brief Utility class to handle reading files from a Scratch .sb3 archive (which is a ZIP file)
 * using the Minizip-ng library.
 * * This class abstracts the low-level file access and memory management for ZIP archives.
 */
class ScratchZipReader {
public:
    ScratchZipReader();
    virtual ~ScratchZipReader();

    /**
     * @brief Opens the Scratch archive file for reading.
     * @param filepath The path to the .sb3 or .zip file.
     * @return true on success, false on failure.
     */
    bool openArchive(const std::string& filepath);

    /**
     * @brief Reads a specific file entry from the opened archive into a string buffer.
     * @param entryName The name of the file inside the ZIP (e.g., "project.json").
     * @param outputBuffer Reference to a string to hold the content.
     * @return true on success, false on entry not found or read error.
     */
    bool readEntryToString(const std::string& entryName, std::string& outputBuffer);
    
    /**
     * @brief Reads a specific file entry from the opened archive into a void* heap buffer.
     * This is useful for passing data directly to memory-loading functions like SDL_RWFromMem.
     * The caller is responsible for freeing the returned pointer using mz_free, which is provided
     * by minizip-ng for consistent memory handling.
     * * @param entryName The name of the file inside the ZIP (e.g., "sprite.png").
     * @param outputSize Reference to store the size of the extracted data.
     * @return Pointer to the extracted data on the heap, or nullptr on failure.
     */
    void* readEntryToHeap(const std::string& entryName, size_t& outputSize);

    /**
     * @brief Closes the open archive handle.
     */
    void closeArchive();

private:
    void* zip_handle_; // Opaque pointer used by minizip-ng API
};