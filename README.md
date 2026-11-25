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
Gugluu-0.2.0 - Copy/
├── Gugluu/
│   ├── src/
│   │   ├── Source.cpp              # Application entry point
│   │   ├── DataLoader.cpp/.h       # File discovery, cache, and allowed extensions
│   │   ├── FileRecord.cpp/.h       # Per-file text, metadata, and embeddings
│   │   ├── Embedding.h             # Chunk pooling and embedding normalization
│   │   ├── similarity.h            # Cosine-similarity ranking
│   │   ├── ui.h                    # Console prompts and result output
│   │   ├── Tokenizer/              # Custom BERT WordPiece tokenizer
│   │   └── ONNX/                   # ONNX Runtime model session
│   └── Vendor/                     # Downloaded ONNX Runtime and model files
├── scripts/
│   └── setup_dependencies.py      # Verified dependency setup and restore
├── Build/                          # Dependency build/download workspace
├── AllowedFileTypes.txt            # Indexed file extensions
├── GenerateProjectVS.bat           # Dependency setup and Visual Studio generation
├── premake5.lua                    # Visual Studio project generation config
└── Gugluu.sln                      # Generated Visual Studio solution
```

`Gugluu.sln`, Visual Studio project files, the `Vendor` directory, build output, DLLs, and `.gdata` caches are local/generated assets and are not expected to be committed.

## Requirements

- Windows 10 or later, on x64
- Visual Studio 2026
- The **Desktop development with C++** workload
- MSVC C++ toolset v145
- Python 3.8 or later
- Internet access for the first dependency setup

The setup script downloads and SHA-256 verifies:

- ONNX Runtime 1.31.1 for Windows x64
- `all-MiniLM-L6-v2/model.onnx`
- The matching vocabulary and tokenizer assets

## Setup and build

From the repository root, run:

```bat
GenerateProjectVS.bat
```

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

## Embedding and cache behavior

Gugluu chunks text into sequences of up to 256 tokens. It averages valid token vectors for each chunk, normalizes each chunk, averages the chunk embeddings for a file, and normalizes the final file embedding.

The 384-dimensional query embedding is compared directly against cached or newly generated file embeddings using dot-product/cosine similarity.

The persisted cache is written beside the running executable as:

```text
Data.gdata
```

The cache stores file paths, file sizes, modification times, IDs, and embeddings. Gugluu reloads it, re-embeds files whose size or modification time changed, removes entries for missing files during a complete scan, and rewrites the cache after indexing.

Delete `Data.gdata` to force a fresh index. If the cache reports an invalid format or invalid embeddings, delete it and rerun Gugluu.

## Troubleshooting

- Run `python scripts\setup_dependencies.py --check` to verify model and ONNX Runtime assets without downloading them.
- Run `GenerateProjectVS.bat` again if `Gugluu.sln` or project files are missing or stale.
- Build for **x64**; Gugluu's ONNX Runtime dependency is Windows x64.
- Ensure `AllowedFileTypes.txt`, `onnxruntime.dll`, `onnxruntime_providers_shared.dll`, `model.onnx`, and `vocab.txt` are available beside or relative to the executable.
- If a directory cannot be indexed, confirm that the path is a valid directory and accessible to the process.
- If the first run is slow, allow time for the initial file embedding pass and model startup.

## Core technologies

- C++20
- Premake 5
- ONNX Runtime 1.31.1
- `sentence-transformers/all-MiniLM-L6-v2`
- Microsoft Visual C++ / MSBuild
