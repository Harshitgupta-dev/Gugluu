# Gugluu 0.2.0

Gugluu 0.2.0 is a native Windows 64-bit, console-based semantic file search engine written in C++20.

It recursively indexes files in a selected directory, generates sentence embeddings with the `all-MiniLM-L6-v2` ONNX model, and ranks matching files with cosine similarity. Gugluu uses a custom BERT WordPiece tokenizer and ONNX Runtime; it is not a keyword-only search implementation.

## What Gugluu does

1. Accepts a directory to index as a command-line argument or prompts for one.
2. Recursively scans regular files and supported symlinks.
3. Reads file text and filters files by extensions in `AllowedFileTypes.txt`.
4. Splits text into 256-token chunks and creates a 384-dimensional embedding for each chunk.
5. Prompts for a search query and creates an embedding for it.
6. Computes cosine similarity with each indexed file's averaged embedding.
7. Prints the highest-ranked files, up to 10 results.

## Project structure

```text
Gugluu
│
├── Gugluu/
│   ├── src/
│   └── Vendor/ 
│                    
├── scripts/
│   └── setup_dependencies.py 
│    
├── Build/
│
├── AllowedFileTypes.txt
├── premake5.lua
│
└── vendor/bin/premake 
```
The setup script downloads and SHA-256 verifies:

- ONNX Runtime 1.31.1 for Windows x64
- `all-MiniLM-L6-v2/model.onnx`
- The matching vocabulary and tokenizer assets
- Installs and build FAISS and openBLAS used in program
- installs logging library spdlog

## Setup and build

From the build folder run Build.bat or Build.sh according to the system you are building the project for. 

This batch file:

1. Runs `python scripts\setup_dependencies.py`.
2. Downloads and verifies missing model and ONNX Runtime assets.
3. Runs the bundled Premake executable.
4. Generates the Visual Studio solution for Visual Studio 2026.

Then open `Gugluu.sln`, choose the `x64` platform and `Debug` or `Release`, and build the `Gugluu` project or solution.

You can also generate and verify dependencies manually:

```bat
python scripts\setup_dependencies.py
python scripts\setup_dependencies.py --check
```

Or build the generated solution from a Developer Command Prompt:

```bat
msbuild Gugluu.sln /m /p:Configuration=Release /p:Platform=x64
```

Build output:

```text
Bin\Release-windows-x86_64\Gugluu\Gugluu.exe
```

Use `Debug-windows-x86_64` for a Debug build.

## Run Gugluu

Pass a directory as the first argument:

```bat
Bin\Release-windows-x86_64\Gugluu\Gugluu.exe "C:\path\to\folder"
```

You may also run it without arguments and enter the directory when prompted:

```text
Directory to index: C:\path\to\folder
Search: find notes about databases
Indexing files...
Top 10 matches:
C:\path\to\folder\database-notes.txt (score: 0.873412)
C:\path\to\folder\overview.md (score: 0.724891)
Searched 42 files.
Completed in 1250 ms.
```

Only one directory argument is accepted. Running with more than one argument exits with a usage error.

## Indexed file types

`AllowedFileTypes.txt` controls extension filtering. The default configuration includes source code, text, Markdown, JSON, XML, CSS, and HTML files:

```text
.h
.hpp
.c
.cpp
.py
.js
.java
.txt
.md
.json
.xml
.css
.html
```

Each configured extension must start with `.` and contain only letters, digits, `_`, or `-`. Lines beginning with `#` are ignored.

## Embeddings
Files are break into chunks of 256 tokens and then converted into 384 dimensional vector as embeddings. then cosine similarity is used and most relevant files are found by considering the similarity of those chunks to the query ranking them.

## Cache behavior
The persisted cache is written beside the running executable as:

```text
Data.gdata
```

The cache stores file paths, file sizes, modification times, IDs, and embeddings. Gugluu reloads it, re-embeds files whose size or modification time changed, removes entries for missing files during a complete scan, and rewrites the cache after indexing.

Delete `Data.gdata` to force a fresh index. If the cache reports an invalid format or invalid embeddings, delete it and rerun Gugluu.
