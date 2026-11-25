# Gugluu

Gugluu is a command-line semantic search application. It scans a directory for supported files, tokenizes their contents, generates embeddings with the `all-MiniLM-L6-v2` ONNX model, and uses FAISS to find relevant files. Supported file extensions are configured in `AllowedFileTypes.txt`.

## Generate a project

Run the launcher from the repository root. It checks and prepares the required dependencies, asks which project format to generate, then stops; it does not compile Gugluu or create an executable.

```text
Build/build.bat
```

```sh
sh Build/build.sh
```

On Windows, choose Visual Studio 2026 to generate `Gugluu.slnx` (and the Premake Visual Studio project) in the repository root, or choose another listed generator. The Visual Studio 2022 option generates `Gugluu.sln`. You can also choose a generator without a prompt, for example `Build/build.bat vs2026` or `sh Build/build.sh gmake2`.

The setup script downloads and verifies ONNX Runtime and the model/tokenizer, and checks the vendored spdlog headers and staged FAISS/OpenBLAS libraries. It builds FAISS and OpenBLAS only when their staged artifacts are missing or stale; that recovery path uses CMake and requires a C++ toolchain. Premake project generation itself does not use CMake. Premake is downloaded automatically if it is not already installed.

The generated project uses the existing `Gugluu/Vendor` dependency layout and writes generated project files to the repository root. Build Gugluu later from the generated project using the selected editor/build system. The executable, when built, is placed in `Bin/<Configuration>-<System>-<Architecture>/Gugluu`.

Supported ONNX Runtime targets are Windows x64, Linux x64/ARM64, and Apple Silicon macOS. FAISS uses OpenMP; install a compatible OpenMP runtime when it is not included with the compiler, particularly on macOS.

## Run

Run the generated `Gugluu` executable from its `Bin` output folder, choose a directory to index, then enter search queries. Type `exit` to quit. The model, tokenizer, allowed-file-types list, and linked libraries are read from their existing locations under the repository; build outputs do not duplicate them. On Windows, launch through Visual Studio to get the configured ONNX Runtime library path, or add `Gugluu/Vendor/ONNX/lib` to `PATH` before launching the executable directly. The index is stored as `Data.gdata` next to the executable. Supported extensions are listed in `AllowedFileTypes.txt`.

## Repository layout

```text
Build/build.bat and Build/build.sh   Dependency setup and project generation
premake5.lua                         Gugluu application project definition
scripts/setup_dependencies.py        Dependency checks/downloads and project generation
Gugluu/src/                           Application source
Gugluu/Vendor/spdlog/                 Vendored logging library
Gugluu/Vendor/FAISS/                  Configuration-specific FAISS libraries and headers
Gugluu/Vendor/openBLAS/               Configuration-specific OpenBLAS libraries and headers
Gugluu/Vendor/ONNX/                   Downloaded ONNX Runtime
Gugluu/Vendor/Model/                  Downloaded model/tokenizer assets
vendor/bin/                           Cached Premake executable
Build/                                Dependency download and FAISS/OpenBLAS build cache
  faiss-int/                           FAISS CMake intermediate files
  openblas-int/                        OpenBLAS CMake intermediate files
Bin/                                  Executables and debug symbols only
```

Generated builds, staged dependencies, downloaded assets, and local index files are excluded by `.gitignore`.
