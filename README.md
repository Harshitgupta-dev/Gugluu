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
└── Vendor/Bin/Premake/
```
The setup script downloads and SHA-256 verifies:

- ONNX Runtime 1.31.1 for Windows x64
- `all-MiniLM-L6-v2/model.onnx`
- The matching vocabulary and tokenizer assets
- Installs and build FAISS and openBLAS used in program
- installs logging library spdlog

## Setup and build

From the build folder run Build.bat or Build.sh according to the system you are building the project for. This will provide you with project files according to your system like .slnx if u choose visual studio.

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
# Gugluu

Gugluu is a local semantic file-search application written in C++20. It creates text embeddings with the all-MiniLM-L6-v2 ONNX model and ranks files by cosine similarity.

## Requirements

- Windows x64
- Visual Studio 2026 with the C++ desktop workload
- Python 3.8 or newer available as `python` on `PATH` (the setup script uses only the standard library)
- Internet access the first time dependencies need to be restored

Premake 5 and its `vs2026` generator are included in `Vendor/Bin/Premake` alongside the Premake license.

## Generate and build

From the repository root, run `GenerateProjectVS.bat`. It runs `scripts/setup_dependencies.py` first to verify the model and ONNX Runtime dependencies, downloading missing or invalid files into `Gugluu/Vendor` as needed. The setup script uses the pinned all-MiniLM-L6-v2 model revision and ONNX Runtime 1.31.1 Windows x64 release; downloaded files are checked against SHA-256 digests before being installed. Existing valid files are left in place. The vendor directory is intentionally excluded from Git, so a fresh clone must run this setup before building.

After dependency setup succeeds, the batch file invokes Premake's `vs2026` action to generate the Visual Studio project and `Gugluu.slnx`. Open the solution and build the Debug or Release x64 configuration. The build copies the model assets, `AllowedFileTypes.txt`, and ONNX Runtime DLLs beside the executable.

To check whether dependencies are ready without downloading or changing anything, run:

```text
python scripts/setup_dependencies.py --check
```

## Run

Run `Gugluu.exe [directory-to-index]`. If the directory is omitted, Gugluu prompts for it. Gugluu then prompts for a search query. The selected directory is scanned recursively; symbolic links to files are considered, but directory links are not traversed.

`AllowedFileTypes.txt` is stored beside the repository root during development and copied beside the executable for packaged runs. Put one dot-prefixed extension per line (for example, `.cpp`). Blank lines and lines beginning with `#` are ignored. Extensions are matched case-insensitively. Missing or invalid configuration is reported as an error.

The `Data.gdata` index cache is stored beside the executable. It is a regenerable local cache; do not treat it as a portable backup.

## Current limitations

- Search and indexing run in one console process; there is no background watcher or interactive multi-query session.
- The current tokenizer is byte-oriented and is not a complete Unicode BERT tokenizer.
- The cache format is application-specific and has no migration mechanism yet.
- Search results are ranked against an average normalized file embedding, so very long files are represented coarsely.
