#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

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

std::string classifyFile(const std::string& extension)
{
    // Text-based files
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

    // Documents that need special extraction
    if (extension == ".pdf" ||
        extension == ".docx")
    {
        return "DOCUMENT";
    }

    // Image files
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
    std::string filename;

    std::cout << "Enter the path to a file: ";
    std::getline(std::cin, filename);

    std::filesystem::path filePath(filename);

    // Check if the file exists
    if (!std::filesystem::exists(filePath))
    {
        std::cout << "File does not exist." << std::endl;
        return 1;
    }

    // Create information about the file
    FileInfo info;

    info.name = filePath.filename().string();
    info.extension = filePath.extension().string();
    info.size = std::filesystem::file_size(filePath);
    info.path = filename;

    // Classify the file
    std::string category = classifyFile(info.extension);

    // Display file information
    std::cout << "\n--- File Information ---\n";

    std::cout << "File name: "
        << info.name << std::endl;

    std::cout << "Extension: "
        << info.extension << std::endl;

    std::cout << "Size: "
        << info.size << " bytes" << std::endl;

    std::cout << "Category: "
        << category << std::endl;

    // Only try to read text-based files
    if (category == "TEXT")
    {
        std::string contents = readFile(filename);

        if (contents.empty())
        {
            std::cout << "Could not read the file." << std::endl;
            return 1;
        }

        std::cout << "\n--- File Contents ---\n";
        std::cout << contents;
    }
    else if (category == "DOCUMENT")
    {
        std::cout << "\nThis document requires a specialized text extractor."
            << std::endl;
    }
    else if (category == "IMAGE")
    {
        std::cout << "\nThis is an image file."
            << std::endl;
    }
    else
    {
        std::cout << "\nFile type is not currently supported."
            << std::endl;
    }

    return 0;
}