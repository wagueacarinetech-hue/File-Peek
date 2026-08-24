# File-Peek
A C++ file intelligence tool that uses NVIDIA Nemotron to help users quickly understand the contents of local documents.
# FilePeek

FilePeek is a C++ project that explores a faster way to understand the contents of local files.

## Motivation

When users want to quickly understand an unfamiliar document, they often have to open the file, copy its contents, switch to an AI assistant, paste the text, and request a summary. FilePeek aims to explore a more seamless workflow by bringing AI-powered document understanding closer to the local file environment.

## Project Goal

The goal is to build a C++ application that can inspect local files, extract their text, and generate concise summaries using NVIDIA Nemotron.

The long-term vision is to explore integration with the user's native file-browsing workflow, allowing users to quickly understand a file without manually copying and pasting its contents into a separate AI application.

## Planned Development

FilePeek will be developed incrementally:

1. Build a C++ program that reads local `.txt` files.
2. Add filesystem interaction and file inspection.
3. Integrate NVIDIA Nemotron for summarization.
4. Handle long documents through chunking.
5. Add support for additional document formats.
6. Implement summary caching and file-change detection.
7. Explore asynchronous/background processing.
8. Build a user interface.
9. Explore native file-manager integration.

## Status

🚧 Early development — currently setting up the project and building the C++skills.
