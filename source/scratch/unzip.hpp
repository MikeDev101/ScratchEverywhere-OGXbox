#pragma once

#include "interpret.hpp"

// Minizip-NG headers
#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_buf.h"
#include "mz_strm_mem.h"
#include "mz_strm_os.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"

#include "os.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#ifdef __NDS__
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

class Unzip {
  public:
    static volatile int projectOpened;
    static std::string loadingState;
    static volatile bool threadFinished;
    static std::string filePath;
    static bool UnpackedInSD;
    
    // Minizip-ng uses void* handles for readers/writers
    static void* zip_reader; 
    static std::vector<char> zipBuffer;

    static void openScratchProject(void *arg) {
        loadingState = "Opening Scratch project";
        Unzip::UnpackedInSD = false;
        std::istream *file = nullptr;

        int isFileOpen = openFile(file);
        if (isFileOpen == 0) {
            Log::logError("Failed to open Scratch project.");
            Unzip::projectOpened = -1;
            Unzip::threadFinished = true;
            return;
        } else if (isFileOpen == -1) {
            Log::log("Main Menu activated.");
            Unzip::projectOpened = -3;
            Unzip::threadFinished = true;
            return;
        }
        loadingState = "Unzipping Scratch project";
        nlohmann::json project_json = unzipProject(file);
        if (project_json.empty()) {
            Log::logError("Project.json is empty.");
            Unzip::projectOpened = -2;
            Unzip::threadFinished = true;
            delete file;
            return;
        }
        loadingState = "Loading Sprites";
        loadSprites(project_json);
        Unzip::projectOpened = 1;
        Unzip::threadFinished = true;
        delete file;
        return;
    }

#ifdef __NDS__ 
    // ... (NDS implementation remains unchanged)
    static std::vector<std::string> getProjectFiles(const std::string &directory) {
        std::vector<std::string> projectFiles;
        struct stat dirStat;

        if (stat(directory.c_str(), &dirStat) != 0) {
            Log::logWarning("Directory does not exist! " + directory);
            if (mkdir(directory.c_str(), 0777) != 0) {
                Log::logWarning("Failed to create directory: " + directory);
            }
            return projectFiles;
        }

        if (!S_ISDIR(dirStat.st_mode)) {
            Log::logWarning("Path is not a directory! " + directory);
            return projectFiles;
        }

        DIR *dir = opendir(directory.c_str());
        if (!dir) {
            Log::logWarning("Failed to open directory: " + std::string(strerror(errno)));
            return projectFiles;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            std::string fullPath = directory + "/" + entry->d_name;
            struct stat fileStat;
            if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && strcmp(ext, ".sb3") == 0) {
                    projectFiles.push_back(entry->d_name);
                }
            }
        }

        closedir(dir);
        return projectFiles;
    }
#else
    static std::vector<std::string> getProjectFiles(const std::string &directory) {
        std::vector<std::string> projectFiles;

        if (!std::filesystem::exists(directory)) {
            Log::logWarning("Directory does not exist! " + directory);
            try {
                std::filesystem::create_directory(directory);
            } catch (...) {
            }
            return projectFiles;
        }

        if (!std::filesystem::is_directory(directory)) {
            Log::logWarning("Path is not a directory! " + directory);
            return projectFiles;
        }

        try {
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".sb3") {
                    projectFiles.push_back(entry.path().filename().string());
                }
            }
        } catch (const std::filesystem::filesystem_error &e) {
            Log::logWarning(std::string("Failed to open directory: ") + e.what());
        }

        return projectFiles;
    }
#endif

    static std::string getSplashText() {
        std::string textPath = "gfx/menu/splashText.txt";
        textPath = OS::getRomFSLocation() + textPath;

        std::vector<std::string> splashLines;
        std::ifstream file(textPath);

        if (!file.is_open()) return "Everywhere!"; 

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) splashLines.push_back(line);
        }
        file.close();

        if (splashLines.empty()) return "Everywhere!"; 

        // Initialize random number generator with current time
        static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_int_distribution<size_t> dist(0, splashLines.size() - 1);

        std::string splash = splashLines[dist(rng)];

        // Replace {PlatformName} with OS::getPlatform()
        const std::string platformName = "{PlatformName}";
        const std::string platform = OS::getPlatform();

        size_t pos = 0;
        while ((pos = splash.find(platformName, pos)) != std::string::npos) {
            splash.replace(pos, platformName.size(), platform);
            pos += platform.size(); // move past replacement
        }
        return splash;
    }

    static nlohmann::json unzipProject(std::istream *file) {
        nlohmann::json project_json;

        if (projectType != UNZIPPED) {
            // 1. Read the file into memory buffer
            Log::log("Reading SB3...");
            std::streamsize size = file->tellg();
            file->seekg(0, std::ios::beg);
            zipBuffer.resize(size);
            if (!file->read(zipBuffer.data(), size)) {
                return project_json;
            }

            // 2. Create Mem Stream and Zip Reader
            Log::log("Opening SB3 file...");
            
            // FIX: Assign the result, don't pass address
            void* mem_stream = mz_stream_mem_create(); 
            
            mz_stream_mem_set_buffer(mem_stream, zipBuffer.data(), (int32_t)zipBuffer.size());

            // FIX: Assign the result
            zip_reader = mz_zip_reader_create(); 

            if (mz_zip_reader_open(zip_reader, mem_stream) != MZ_OK) {
                Log::logError("Failed to open zip stream");
                mz_stream_mem_delete(&mem_stream);
                return project_json;
            }

            // 3. Locate project.json
            Log::log("Extracting project.json...");
            if (mz_zip_reader_locate_entry(zip_reader, "project.json", 0) != MZ_OK) {
                Log::logError("project.json not found in zip");
                return project_json;
            }

            // 4. Open Entry
            if (mz_zip_reader_entry_open(zip_reader) != MZ_OK) {
                Log::logError("Failed to open project.json entry");
                return project_json;
            }

            // 5. Get Info (Size)
            mz_zip_file *file_info = nullptr;
            mz_zip_reader_entry_get_info(zip_reader, &file_info);
            
            // 6. Read Data
            std::string json_str;
            json_str.resize(file_info->uncompressed_size);
            mz_zip_reader_entry_read(zip_reader, (void*)json_str.data(), (int32_t)json_str.size());
            
            // 7. Close Entry
            mz_zip_reader_entry_close(zip_reader);

#ifdef ENABLE_CLOUDVARS
            projectJSON = json_str;
#endif

            // Parse JSON
            Log::log("Parsing project.json...");
            project_json = nlohmann::json::parse(json_str);
            
            // Note: We do not close the zip_reader here because we likely need it 
            // to extract assets later (sprites, sounds, etc.)
            // Ensure you clean up zip_reader and mem_stream when completely done.

        } else {
            // (Standard file read logic remains same)
            file->clear();
            file->seekg(0, std::ios::beg);
            file->seekg(0, std::ios::end);
            std::streamsize size = file->tellg();
            file->seekg(0, std::ios::beg);

            std::string json_content;
            json_content.reserve(size);
            json_content.assign(std::istreambuf_iterator<char>(*file),
                                std::istreambuf_iterator<char>());

#ifdef ENABLE_CLOUDVARS
            projectJSON = json_content;
#endif
            project_json = nlohmann::json::parse(json_content);
        }
        return project_json;
    }

    static int openFile(std::istream *&file);
    static bool load();

    static bool extractProject(const std::string &zipPath, const std::string &destFolder) {
        // FIX: Assign the result
        void* reader = mz_zip_reader_create();

        if (mz_zip_reader_open_file(reader, zipPath.c_str()) != MZ_OK) {
            Log::logError("Failed to open zip: " + zipPath);
            mz_zip_reader_delete(&reader);
            return false;
        }

        // Iterate through all files
        int32_t err = mz_zip_reader_goto_first_entry(reader);
        while (err == MZ_OK) {
            mz_zip_file *file_info = nullptr;
            mz_zip_reader_entry_get_info(reader, &file_info);

            std::string filename = file_info->filename;
            
            // Security check
            if (filename.find("..") == std::string::npos) { 
                std::string outPath = destFolder + "/" + filename;
                
                // Ensure directory exists
                std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

                // Open entry
                if (mz_zip_reader_entry_open(reader) == MZ_OK) {
                    // Write to file
                    std::ofstream outfile(outPath, std::ios::binary);
                    if (outfile.is_open()) {
                         // Buffer for reading chunks
                        const int buf_size = 4096;
                        std::vector<char> buf(buf_size);
                        int32_t read_bytes;
                        
                        while ((read_bytes = mz_zip_reader_entry_read(reader, buf.data(), buf_size)) > 0) {
                            outfile.write(buf.data(), read_bytes);
                        }
                        outfile.close();
                    }
                    mz_zip_reader_entry_close(reader);
                }
            }
            err = mz_zip_reader_goto_next_entry(reader);
        }

        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return true;
    }

    static bool deleteProjectFolder(const std::string &directory) {
        if (!std::filesystem::exists(directory)) {
            Log::logWarning("Directory does not exist: " + directory);
            return false;
        }

        if (!std::filesystem::is_directory(directory)) {
            Log::logWarning("Path is not a directory: " + directory);
            return false;
        }

        try {
            std::filesystem::remove_all(directory);
            return true;
        } catch (const std::filesystem::filesystem_error &e) {
            Log::logError(std::string("Failed to delete folder: ") + e.what());
            return false;
        }
    }

    static nlohmann::json getSetting(const std::string &settingName) {
        std::string folderPath = filePath + ".json";

        std::ifstream file(folderPath);
        if (!file.good()) {
            Log::logWarning("Project settings file not found: " + folderPath);
            return nlohmann::json();
        }

        nlohmann::json json;
        try {
            file >> json;
        } catch (const nlohmann::json::parse_error &e) {
            Log::logError("Failed to parse JSON file: " + std::string(e.what()));
            file.close();
            return nlohmann::json();
        }
        file.close();

        if (!json.contains("settings")) {
            return nlohmann::json();
        }
        if (!json["settings"].contains(settingName)) {
            return nlohmann::json();
        }

        return json["settings"][settingName];
    }
};

// Define static members
inline volatile int Unzip::projectOpened = 0;
inline std::string Unzip::loadingState = "";
inline volatile bool Unzip::threadFinished = false;
inline std::string Unzip::filePath = "";
inline bool Unzip::UnpackedInSD = false;
inline void* Unzip::zip_reader = nullptr;
inline std::vector<char> Unzip::zipBuffer;