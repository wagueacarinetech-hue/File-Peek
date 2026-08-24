#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdio>
#include <windows.h>
#include <functional>

struct FileInfo
{
    std::string name;
    std::string extension;
    std::uintmax_t size;
    std::string path;
};

std::string readFile(const std::string& filename)
{
    std::ifstream file(filename);

    if (!file)
    {
        return "";
    }

    std::string contents;
    std::string line;

    while (std::getline(file, line))
    {
        contents += line + "\n";
    }

    return contents;
}

// use python for pdf and docx
std::string extractDocument(const std::string& filename)
{
    std::string command =
        "python extractor.py \"" + filename + "\" 2>&1";

    FILE* pipe = _popen(command.c_str(), "r");

    if (!pipe)
    {
        return "";
    }

    std::string result;
    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }

    int exitCode = _pclose(pipe);

    if (exitCode != 0)
    {
        return "";
    }

    return result;
}

// make a cache name for each file and mode
std::string getCachePath(
    const std::string& filename,
    const std::string& mode)
{
    std::filesystem::create_directory(".filepeek_cache");

    std::string fullPath =
        std::filesystem::absolute(filename).string();

    auto modified =
        std::filesystem::last_write_time(filename)
        .time_since_epoch()
        .count();

    std::string cacheKey =
        fullPath + std::to_string(modified) + mode;

    std::size_t hash =
        std::hash<std::string>{}(cacheKey);

    return ".filepeek_cache/" +
        std::to_string(hash) +
        "_" +
        mode +
        ".txt";
}

// check if we already summarized this version
std::string readCache(
    const std::string& filename,
    const std::string& mode)
{
    std::string cachePath =
        getCachePath(filename, mode);

    if (!std::filesystem::exists(cachePath))
    {
        return "";
    }

    return readFile(cachePath);
}

// save the summary for next time
void saveCache(
    const std::string& filename,
    const std::string& mode,
    const std::string& summary)
{
    std::string cachePath =
        getCachePath(filename, mode);

    std::ofstream file(cachePath, std::ios::binary);

    if (file)
    {
        file.write(summary.data(), summary.size());
    }
}

// send the text to the summarizer
std::string summarizeText(
    const std::string& text,
    const std::string& mode)
{
    std::string tempFile = "filepeek_temp.txt";

    std::ofstream output(tempFile, std::ios::binary);

    if (!output)
    {
        return "ERROR: Could not create temporary file.";
    }

    output.write(text.data(), text.size());
    output.close();

    std::string command =
        "python summarizer.py \"" +
        tempFile +
        "\" " +
        mode +
        " 2>&1";

    FILE* pipe = _popen(command.c_str(), "r");

    if (!pipe)
    {
        std::filesystem::remove(tempFile);
        return "ERROR: Could not start summarizer.";
    }

    std::string summary;
    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        summary += buffer;
    }

    int exitCode = _pclose(pipe);

    // we dont need this file anymore
    std::filesystem::remove(tempFile);

    if (exitCode != 0)
    {
        return summary;
    }

    return summary;
}

std::string classifyFile(const std::string& extension)
{
    if (extension == ".txt" ||
        extension == ".md" ||
        extension == ".csv" ||
        extension == ".log" ||
        extension == ".cpp" ||
        extension == ".h" ||
        extension == ".hpp" ||
        extension == ".py" ||
        extension == ".java" ||
        extension == ".js" ||
        extension == ".html" ||
        extension == ".css" ||
        extension == ".json" ||
        extension == ".xml")
    {
        return "TEXT";
    }

    if (extension == ".pdf" ||
        extension == ".docx")
    {
        return "DOCUMENT";
    }

    if (extension == ".jpg" ||
        extension == ".jpeg" ||
        extension == ".png" ||
        extension == ".gif")
    {
        return "IMAGE";
    }

    return "UNKNOWN";
}

int main()
{
    // fixes weird characters in the console
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::string filename;

    std::cout << "Enter the path to a file: ";
    std::getline(std::cin, filename);

    std::filesystem::path filePath(filename);

    if (!std::filesystem::exists(filePath))
    {
        std::cout << "File does not exist." << std::endl;
        return 1;
    }

    FileInfo info;

    info.name = filePath.filename().string();
    info.extension = filePath.extension().string();
    info.size = std::filesystem::file_size(filePath);
    info.path = filename;

    std::string category =
        classifyFile(info.extension);

    std::cout << "\n--- File Information ---\n";
    std::cout << "File name: "
        << info.name << std::endl;
    std::cout << "Extension: "
        << info.extension << std::endl;
    std::cout << "Size: "
        << info.size << " bytes" << std::endl;
    std::cout << "Category: "
        << category << std::endl;

    if (category == "IMAGE")
    {
        std::cout
            << "\nImage support is not implemented yet."
            << std::endl;

        return 0;
    }

    if (category == "UNKNOWN")
    {
        std::cout
            << "\nFile type is not currently supported."
            << std::endl;

        return 0;
    }

    std::cout << "\nFilePeek options:\n";
    std::cout << "1. Quick summary\n";
    std::cout << "2. Detailed summary\n";
    std::cout << "3. Exit\n";
    std::cout << "Choice: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "3")
    {
        return 0;
    }

    std::string mode;

    if (choice == "2")
    {
        mode = "detailed";
    }
    else
    {
        mode = "quick";
    }

    // try the saved summary first
    std::string summary =
        readCache(filename, mode);

    if (!summary.empty())
    {
        std::cout << "\nUsing saved summary..." << std::endl;
    }
    else
    {
        std::string contents;

        if (category == "TEXT")
        {
            contents = readFile(filename);
        }
        else if (category == "DOCUMENT")
        {
            std::cout
                << "\nExtracting document..."
                << std::endl;

            contents = extractDocument(filename);
        }

        if (contents.empty())
        {
            std::cout
                << "Could not get text from the file."
                << std::endl;

            return 1;
        }

        std::cout
            << "\nGenerating "
            << mode
            << " summary with Nemotron..."
            << std::endl;

        summary = summarizeText(contents, mode);

        // dont save errors
        if (summary.rfind("ERROR:", 0) != 0)
        {
            saveCache(filename, mode, summary);
        }
    }

    std::cout << "\n--- FilePeek Summary ---\n";
    std::cout << summary << std::endl;

    // quick summary can be expanded if wanted
    if (mode == "quick")
    {
        std::cout
            << "\nWould you like a detailed summary? (y/n): ";

        std::string answer;
        std::getline(std::cin, answer);

        if (answer == "y" || answer == "Y")
        {
            std::string detailedSummary =
                readCache(filename, "detailed");

            if (!detailedSummary.empty())
            {
                std::cout
                    << "\nUsing saved detailed summary..."
                    << std::endl;
            }
            else
            {
                std::string contents;

                if (category == "TEXT")
                {
                    contents = readFile(filename);
                }
                else
                {
                    std::cout
                        << "\nExtracting document..."
                        << std::endl;

                    contents =
                        extractDocument(filename);
                }

                if (contents.empty())
                {
                    std::cout
                        << "Could not get text from the file."
                        << std::endl;

                    return 1;
                }

                std::cout
                    << "\nGenerating detailed summary..."
                    << std::endl;

                detailedSummary =
                    summarizeText(contents, "detailed");

                if (detailedSummary.rfind("ERROR:", 0) != 0)
                {
                    saveCache(
                        filename,
                        "detailed",
                        detailedSummary
                    );
                }
            }

            std::cout
                << "\n--- Detailed FilePeek Summary ---\n";

            std::cout
                << detailedSummary
                << std::endl;
        }
    }

    return 0;
}