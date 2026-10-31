# Gugluu

Gugluu is a local semantic file-search application written in C++20. It creates text embeddings with the all-MiniLM-L6-v2 ONNX model and ranks files by cosine similarity.

## Requirements

- Windows x64
- Visual Studio 2026 with the C++ desktop workload
- Python 3.8 or newer available as `python` on `PATH` (the setup script uses only the standard library)
- Internet access the first time dependencies need to be restored

Premake 5 and its `vs2026` generator are included in `vendor/bin/premake`.

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
