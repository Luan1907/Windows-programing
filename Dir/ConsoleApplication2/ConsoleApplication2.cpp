#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <direct.h>

void PrintFileTime(const FILETIME& ft) {
    SYSTEMTIME utc, local;
    FileTimeToSystemTime(&ft, &utc);
    SystemTimeToTzSpecificLocalTime(NULL, &utc, &local);

    std::cout << std::setfill('0')
        << std::setw(2) << local.wDay << "/"
        << std::setw(2) << local.wMonth << "/"
        << local.wYear << " "
        << std::setw(2) << local.wHour << ":"
        << std::setw(2) << local.wMinute;
}

void ListFiles(const std::string& path, bool recursive, int depth = 0) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    std::string searchPath = path + "\\*";

    hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        std::cerr << "[Error] Cannot open directory: " << path
            << " (Error code: " << err << ")" << std::endl;
        return;
    }

    int fileCount = 0, dirCount = 0;

    do {
        std::string fileName = findData.cFileName;

        // Skip "." and ".."
        if (fileName == "." || fileName == "..") {
            continue;
        }

        std::string indent(depth * 2, ' ');

        // Print date/time
        std::cout << indent;
        PrintFileTime(findData.ftLastWriteTime);
        std::cout << "   ";

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dirCount++;
            std::cout << "<DIR>          " << fileName << std::endl;

            if (recursive) {
                ListFiles(path + "\\" + fileName, true, depth + 1);
            }
        }
        else {
            fileCount++;

            // Calculate file size
            ULARGE_INTEGER size;
            size.HighPart = findData.nFileSizeHigh;
            size.LowPart = findData.nFileSizeLow;

            std::cout << std::setw(12) << size.QuadPart << "   "
                << fileName << std::endl;
        }

    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);

    if (fileCount > 0 || dirCount > 0) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "=> " << dirCount << " director"
            << (dirCount == 1 ? "y" : "ies") << ", "
            << fileCount << " file" << (fileCount == 1 ? "" : "s")
            << " in \"" << path << "\"" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::string targetPath;
    bool recursiveMode = false;

    // Check if the user provided a path argument
    if (argc < 2) {
        // If no path is provided, use the current working directory
        char currentDirectory[MAX_PATH];
        if (_getcwd(currentDirectory, sizeof(currentDirectory)) != NULL) {
            targetPath = currentDirectory;
            std::cout << "No directory path was provided." << std::endl;
            std::cout << "Using the current working directory instead: "
                << targetPath << std::endl;
        }
        else {
            std::cerr << "Error: Unable to get the current working directory." << std::endl;
            return 1;
        }
    }
    else {
        // The first argument is the target directory
        targetPath = argv[1];

        // Check for optional parameter "/s" (case-insensitive)
        if (argc >= 3 && (_stricmp(argv[2], "/s") == 0)) {
            recursiveMode = true;
        }
    }

    // Display the configuration to the user
    std::cout << "================================================" << std::endl;
    std::cout << "Target directory : " << targetPath << std::endl;
    std::cout << "Recursive listing : "
        << (recursiveMode ? "Enabled" : "Disabled") << std::endl;
    std::cout << "================================================" << std::endl;

    // Call the directory listing function
    ListFiles(targetPath, recursiveMode);

    return 0;
}