 #!/usr/bin/env python3
"""Prepare Gugluu's pinned dependencies and generate a Premake project."""

from __future__ import annotations

import argparse
import base64
import binascii
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "Gugluu" / "Vendor"
PROJECT_DIR = ROOT / "Gugluu"
BUILD = ROOT / "Build" / "Dependencies"
DOWNLOADS = BUILD / "downloads"
PREMAKE_VERSION = "5.0.0-beta2"
PREMAKE_ARCHIVE_SHA256 = {
	"premake-5.0.0-beta2-windows.zip": "87cfa10ed52fd1f4e835f738ac1033ff302035758671400fec078b700c622c54",
}
ONNX_VERSION = "1.31.1"
MODEL_REVISION = "1110a243fdf4706b3f48f1d95db1a4f5529b4d41"
OPENBLAS_VERSION = "0.3.34"
FAISS_VERSION = "1.15.1"

MODEL_FILES = {
	"model.onnx": ("onnx/model.onnx", "6fd5d72fe4589f189f8ebc006442dbb529bb7ce38f8082112682524616046452"),
	"tokenizer.json": ("tokenizer.json", "be50c3628f2bf5bb5e3a7f17b1f74611b2561a3a27eeab05e5aa30f411572037"),
	"tokenizer_config.json": ("tokenizer_config.json", "acb92769e8195aabd29b7b2137a9e6d6e25c476a4f15aa4355c233426c61576b"),
	"special_tokens_map.json": ("special_tokens_map.json", "303df45a03609e4ead04bc3dc1536d0ab19b5358db685b6f3da123d05ec200e3"),
	"vocab.txt": ("vocab.txt", "07eced375cec144d27c900241f3e339478dec958f92fddbc551f295c992038a3"),
}

# The official NuGet package contains the native headers and binaries for
# each RID. This avoids relying on GitHub release assets.
ONNX_NUGET_ID = "microsoft.ml.onnxruntime"

ONNX_PLATFORMS = {
    ("Windows", "x86_64"): (
        "win-x64",
        [
            "onnxruntime.dll",
            "onnxruntime.lib",
            "onnxruntime_providers_shared.dll",
        ],
    ),
    ("Windows", "aarch64"): (
        "win-arm64",
        [
            "onnxruntime.dll",
            "onnxruntime.lib",
            "onnxruntime_providers_shared.dll",
        ],
    ),
    ("Linux", "x86_64"): (
        "linux-x64",
        [
            "libonnxruntime.so",
            "libonnxruntime_providers_shared.so",
        ],
    ),
    ("Linux", "aarch64"): (
        "linux-arm64",
        [
            "libonnxruntime.so",
            "libonnxruntime_providers_shared.so",
        ],
    ),
    ("Darwin", "arm64"): (
        "osx-arm64",
        [
            "libonnxruntime.dylib",
            "libonnxruntime_providers_shared.dylib",
        ],
    ),
    ("Darwin", "x86_64"): (
        "osx-x64",
        [
            "libonnxruntime.dylib",
            "libonnxruntime_providers_shared.dylib",
        ],
    ),
}


def sha256(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as stream:
		for chunk in iter(lambda: stream.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def download(url: str, destination: Path, expected_hash: str | None = None) -> Path:
	destination.parent.mkdir(parents=True, exist_ok=True)
	if destination.is_file() and (expected_hash is None or sha256(destination) == expected_hash):
		return destination
	temporary = destination.with_suffix(destination.suffix + ".download")
	temporary.unlink(missing_ok=True)
	print(f"Downloading {url}")
	request = urllib.request.Request(url, headers={"User-Agent": "Gugluu-build"})
	try:
		with urllib.request.urlopen(request, timeout=900) as response, temporary.open("wb") as output:
			shutil.copyfileobj(response, output)
		if expected_hash is not None and sha256(temporary) != expected_hash:
			raise RuntimeError(f"SHA-256 verification failed for {url}")
		temporary.replace(destination)
	except Exception:
		temporary.unlink(missing_ok=True)
		raise
	return destination


def nuget_json(url: str) -> dict:
    """Fetch a NuGet V3 JSON resource without requesting gzip transfer encoding."""
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Gugluu-build",
            "Accept": "application/json",
            "Accept-Encoding": "identity",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        raise RuntimeError(
            f"NuGet returned HTTP {error.code} while requesting {url}"
        ) from error
    except urllib.error.URLError as error:
        raise RuntimeError(
            f"Could not contact NuGet while requesting {url}: {error.reason}"
        ) from error
    except (UnicodeDecodeError, ValueError) as error:
        raise RuntimeError(
            f"NuGet returned invalid JSON from {url}"
        ) from error


def get_nuget_package_hash(package_id: str, version: str) -> tuple[str, bytes]:
    """
    Resolve the official package hash through NuGet's V3 registration/catalog
    metadata. NuGet's flat-container .nupkg.sha512 URL is not a valid endpoint.
    """
    service_index = nuget_json("https://api.nuget.org/v3/index.json")

    registration_base = None
    for resource in service_index.get("resources", []):
        resource_types = resource.get("@type", [])
        if isinstance(resource_types, str):
            resource_types = [resource_types]

        if any(
            str(resource_type).startswith("RegistrationsBaseUrl")
            for resource_type in resource_types
        ):
            registration_base = resource.get("@id")
            if registration_base:
                break

    if not registration_base:
        raise RuntimeError(
            "NuGet service index does not advertise a registration resource"
        )

    registration_url = (
        registration_base.rstrip("/")
        + f"/{package_id.lower()}/index.json"
    )
    registration = nuget_json(registration_url)

    matching_leaf = None
    for page in registration.get("items", []):
        page_items = page.get("items")

        # Large registration pages may be external documents rather than
        # inlined in the index.
        if page_items is None:
            page_url = page.get("@id")
            if not page_url:
                continue
            page_items = nuget_json(page_url).get("items", [])

        for leaf in page_items:
            catalog_entry = leaf.get("catalogEntry", {})
            if isinstance(catalog_entry, dict):
                leaf_version = catalog_entry.get("version")
            else:
                leaf_version = None

            if (leaf_version or "").lower() == version.lower():
                matching_leaf = leaf
                break

        if matching_leaf is not None:
            break

    if matching_leaf is None:
        raise RuntimeError(
            f"NuGet registration has no {package_id} version {version}"
        )

    catalog_entry = matching_leaf.get("catalogEntry")

    if isinstance(catalog_entry, str):
        catalog_entry = nuget_json(catalog_entry)
    elif isinstance(catalog_entry, dict):
        catalog_url = catalog_entry.get("@id")
        # Registration summaries may omit the package hash. The catalog
        # document contains packageHash and packageHashAlgorithm.
        if not catalog_entry.get("packageHash") and catalog_url:
            catalog_entry = nuget_json(catalog_url)
    else:
        catalog_url = matching_leaf.get("catalogEntry")
        if not catalog_url:
            raise RuntimeError(
                f"NuGet registration metadata for {package_id} {version} "
                "does not contain a catalog entry"
            )
        catalog_entry = nuget_json(catalog_url)

    algorithm = str(
        catalog_entry.get("packageHashAlgorithm", "")
    ).upper()
    encoded_hash = catalog_entry.get("packageHash")

    if algorithm != "SHA512" or not encoded_hash:
        raise RuntimeError(
            f"NuGet did not provide a SHA512 package hash for "
            f"{package_id} {version} (algorithm={algorithm or 'missing'})"
        )

    try:
        expected_hash = base64.b64decode(encoded_hash, validate=True)
    except (ValueError, binascii.Error) as error:
        raise RuntimeError(
            f"NuGet returned an invalid package hash for {package_id} {version}"
        ) from error

    if len(expected_hash) != hashlib.sha512().digest_size:
        raise RuntimeError(
            f"NuGet returned an invalid SHA512 hash length for "
            f"{package_id} {version}"
        )

    return algorithm, expected_hash


def download_nuget_package(
    url: str,
    destination: Path,
    package_id: str,
    version: str,
) -> Path:
    """Download a NuGet package and verify it against its catalog SHA-512 hash."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    _, expected_hash = get_nuget_package_hash(package_id, version)

    def matches(path: Path) -> bool:
        digest = hashlib.sha512()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.digest() == expected_hash

    if destination.is_file() and matches(destination):
        print(f"Using cached NuGet package {package_id} {version}")
        return destination

    temporary = destination.with_suffix(destination.suffix + ".download")
    temporary.unlink(missing_ok=True)

    print(f"Downloading {url}")
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Gugluu-build",
            "Accept-Encoding": "identity",
        },
    )

    digest = hashlib.sha512()
    try:
        with urllib.request.urlopen(request, timeout=900) as response:
            with temporary.open("wb") as output:
                for chunk in iter(
                    lambda: response.read(1024 * 1024), b""
                ):
                    digest.update(chunk)
                    output.write(chunk)

        if digest.digest() != expected_hash:
            raise RuntimeError(
                f"NuGet SHA-512 verification failed for "
                f"{package_id} {version}"
            )

        temporary.replace(destination)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise

    return destination


def safe_destination(root: Path, name: str) -> Path:
	relative = PurePosixPath(name)
	if relative.is_absolute() or ".." in relative.parts:
		raise RuntimeError(f"Archive contains an unsafe path: {name}")
	destination = (root / Path(*relative.parts)).resolve()
	if not destination.is_relative_to(root.resolve()):
		raise RuntimeError(f"Archive contains an unsafe path: {name}")
	return destination


def extract_archive(archive: Path, destination: Path) -> None:
	shutil.rmtree(destination, ignore_errors=True)
	destination.mkdir(parents=True, exist_ok=True)
	if zipfile.is_zipfile(archive):
		with zipfile.ZipFile(archive) as source:
			for entry in source.infolist():
				safe_destination(destination, entry.filename)
			source.extractall(destination)
		return
	if tarfile.is_tarfile(archive):
		with tarfile.open(archive, "r:*") as source:
			members = source.getmembers()
			for member in members:
				safe_destination(destination, member.name)
				if not (member.isfile() or member.isdir()):
					raise RuntimeError(f"Archive contains an unsupported entry: {member.name}")
			source.extractall(destination, members=members)
		return
	raise RuntimeError(f"Unsupported dependency archive: {archive}")


def host_platform() -> tuple[str, str, str]:
	system = platform.system()
	machine = platform.machine().lower()
	if machine in ("amd64", "x86_64"):
		arch = "x86_64"
	elif machine in ("arm64", "aarch64"):
		arch = "arm64" if system == "Darwin" else "aarch64"
	else:
		raise RuntimeError(f"Unsupported processor: {platform.machine()}")
	platform_name = {"Windows": "windows", "Linux": "linux", "Darwin": "macosx"}.get(system)
	if platform_name is None:
		raise RuntimeError(f"Unsupported operating system: {system}")
	return system, arch, platform_name

def ensure_runtime_assets(system: str, arch: str) -> None:
    platform_entry = ONNX_PLATFORMS.get((system, arch))
    if platform_entry is None:
        raise RuntimeError(
            f"No ONNX Runtime NuGet package for {system}/{arch}"
        )

    rid, runtime_files = platform_entry
    onnx_dir = VENDOR / "ONNX"
    include_dir = onnx_dir / "include"
    lib_dir = onnx_dir / "lib"
    version_file = onnx_dir / "VERSION_NUMBER"

    include_files = [
        "onnxruntime_c_api.h",
        "onnxruntime_cxx_api.h",
        "onnxruntime_cxx_inline.h",
    ]

    api_header = include_dir / "onnxruntime_c_api.h"
    api_header_matches = (
        api_header.is_file()
        and "#define ORT_API_VERSION 31" in api_header.read_text(encoding="utf-8")
    )

    if (
        version_file.is_file()
        and version_file.read_text(encoding="utf-8").strip() == ONNX_VERSION
        and api_header_matches
        and all((include_dir / name).is_file() for name in include_files)
        and all((lib_dir / name).is_file() for name in runtime_files)
    ):
        print(f"Using ONNX Runtime {ONNX_VERSION} ({rid})")
    else:
        package_filename = (
            f"{ONNX_NUGET_ID}.{ONNX_VERSION}.nupkg"
        )
        package_url = (
            "https://api.nuget.org/v3-flatcontainer/"
            f"{ONNX_NUGET_ID}/{ONNX_VERSION}/"
            f"{package_filename}"
        )

        archive = download_nuget_package(
            package_url,
            DOWNLOADS / package_filename,
            ONNX_NUGET_ID,
            ONNX_VERSION,
        )

        package_staging = BUILD / "onnx-nuget-staging"
        extract_archive(archive, package_staging)

        # NuGet native packages use build/native/include. Keep the
        # buildTransitive fallback for package-layout compatibility.
        package_include = (
            package_staging / "build" / "native" / "include"
        )
        if not package_include.is_dir():
            package_include = (
                package_staging / "buildTransitive" / "native" / "include"
            )

        package_runtime = (
            package_staging / "runtimes" / rid / "native"
        )

        missing = [
            str(path.relative_to(package_staging))
            for path in (
                [package_include / name for name in include_files]
                + [package_runtime / name for name in runtime_files]
            )
            if not path.is_file()
        ]
        if missing:
            raise RuntimeError(
                "ONNX Runtime NuGet package is missing required files: "
                + ", ".join(missing)
            )

        # Assemble and validate the replacement before touching the
        # currently installed SDK.
        install_staging = BUILD / "onnx-install-staging"
        shutil.rmtree(install_staging, ignore_errors=True)
        staged_include = install_staging / "include"
        staged_lib = install_staging / "lib"
        shutil.copytree(package_include, staged_include)
        staged_lib.mkdir(parents=True, exist_ok=True)

        for filename in runtime_files:
            shutil.copy2(
                package_runtime / filename,
                staged_lib / filename,
            )

        if not all((staged_include / name).is_file() for name in include_files):
            raise RuntimeError("Staged ONNX Runtime headers failed validation")
        if not all((staged_lib / name).is_file() for name in runtime_files):
            raise RuntimeError("Staged ONNX Runtime libraries failed validation")

        onnx_dir.mkdir(parents=True, exist_ok=True)
        shutil.rmtree(include_dir, ignore_errors=True)
        shutil.rmtree(lib_dir, ignore_errors=True)
        staged_include.replace(include_dir)
        staged_lib.replace(lib_dir)

        # Write the version marker last, only after a complete installation.
        version_file.write_text(
            f"{ONNX_VERSION}\n",
            encoding="utf-8",
        )
        shutil.rmtree(install_staging, ignore_errors=True)
        print(f"Installed ONNX Runtime {ONNX_VERSION} ({rid})")

    model_dir = VENDOR / "Model" / "all-MiniLM-L6-v2"
    model_dir.mkdir(parents=True, exist_ok=True)

    for filename, (remote_path, expected_hash) in MODEL_FILES.items():
        url = (
            "https://huggingface.co/sentence-transformers/"
            "all-MiniLM-L6-v2/resolve/"
            f"{MODEL_REVISION}/{remote_path}"
        )
        download(url, model_dir / filename, expected_hash)



def ensure_spdlog() -> None:
	header = VENDOR / "spdlog" / "include" / "spdlog" / "spdlog.h"
	if header.is_file():
		print("Using vendored spdlog")
		return
	archive = download(
		"https://codeload.github.com/gabime/spdlog/zip/refs/tags/v1.17.0",
		DOWNLOADS / "spdlog-1.17.0.zip",
	)
	staging = BUILD / "spdlog-staging"
	extract_archive(archive, staging)
	source = staging / "spdlog-1.17.0"
	if not (source / "include" / "spdlog" / "spdlog.h").is_file():
		raise RuntimeError("The spdlog archive did not contain its expected headers")
	shutil.rmtree(VENDOR / "spdlog", ignore_errors=True)
	shutil.copytree(source, VENDOR / "spdlog")
	print("Installed spdlog 1.17.0")


def dependency_layout(platform_name: str, arch: str, system: str) -> tuple[Path, Path, str, str]:
	if system == "Windows":
		return VENDOR / "FAISS", VENDOR / "openBLAS", ".lib", ""
	suffix = f"{platform_name}-{arch}"
	return VENDOR / "FAISS" / suffix, VENDOR / "openBLAS" / suffix, ".a", "lib"


def staged_artifacts(base: Path, config: str, library: str, header: str, version: str) -> bool:
	return (
		(base / config / "lib" / library).is_file()
		and (base / config / "include" / header).is_file()
		and (base / config / ".gugluu-version").is_file()
		and (base / config / ".gugluu-version").read_text(encoding="utf-8").strip() == version
	)


def prepare_dependency_source(name: str, version: str, url: str, digest: str) -> Path:
    source = BUILD / "sources" / f"{name}-{version}"
    marker = source / "CMakeLists.txt"
    if marker.is_file():
        return source

    filename = f"{name}-{version}.tar.gz"
    archive = download(url, DOWNLOADS / filename, digest)
    staging = BUILD / "sources" / f"{name}-extract"
    extract_archive(archive, staging)

    candidates = list(staging.glob(f"**/{name}-{version}/CMakeLists.txt"))
    if len(candidates) != 1:
        raise RuntimeError(
            f"{filename} did not contain exactly one expected source tree"
        )

    extracted_source = candidates[0].parent
    source.parent.mkdir(parents=True, exist_ok=True)
    shutil.rmtree(source, ignore_errors=True)
    shutil.copytree(extracted_source, source)
    if not marker.is_file():
        raise RuntimeError(f"Failed to prepare source tree for {name} {version}")

    return source



def run(command: list[str], description: str) -> None:
	print(f"{description}...")
	subprocess.run(command, cwd=ROOT, check=True)


def build_faiss_openblas(platform_name: str, arch: str, system: str) -> None:
	faiss_base, openblas_base, suffix, prefix = dependency_layout(platform_name, arch, system)
	configs = ("Debug", "Release")
	expected = all(
		staged_artifacts(
			openblas_base,
			config.upper() if system == "Windows" else config,
			f"{prefix}openblas{suffix}",
			"openblas/cblas.h",
			f"OpenBLAS {OPENBLAS_VERSION}",
		)
		and staged_artifacts(
			faiss_base,
			config,
			f"{prefix}faiss{suffix}",
			"faiss/IndexFlat.h",
			f"FAISS {FAISS_VERSION}",
		)
		for config in configs
	)
	if expected:
		print(f"Using staged FAISS {FAISS_VERSION} and OpenBLAS {OPENBLAS_VERSION}")
		return

	cmake = shutil.which("cmake")
	if cmake is None:
		raise RuntimeError(
			"FAISS/OpenBLAS are missing. Install CMake and a C++ build toolchain to build them."
		)

	openblas_source = prepare_dependency_source(
		"OpenBLAS",
		OPENBLAS_VERSION,
		f"https://codeload.github.com/OpenMathLib/OpenBLAS/tar.gz/refs/tags/v{OPENBLAS_VERSION}",
		"cd7e129868320cc2d033afa920e31202dfe0b8066a5b66661900ccc0f197dfed",
	)
	faiss_source = prepare_dependency_source(
		"faiss",
		FAISS_VERSION,
		f"https://codeload.github.com/facebookresearch/faiss/tar.gz/refs/tags/v{FAISS_VERSION}",
		"7b583010cc3c0a778b909a1beecbc5a4bd42f415d82af734b2c05d5eaaca0ffa",
	)

	for config in configs:
		openblas_config = config.upper() if system == "Windows" else config
		openblas_install = openblas_base / openblas_config
		faiss_install = faiss_base / config
		openblas_library = openblas_install / "lib" / f"{prefix}openblas{suffix}"
		openblas_header = openblas_install / "include" / "openblas" / "cblas.h"
		if not staged_artifacts(
			openblas_base,
			openblas_config,
			f"{prefix}openblas{suffix}",
			"openblas/cblas.h",
			f"OpenBLAS {OPENBLAS_VERSION}",
		):
			build_dir = BUILD / "openblas-int" / config.lower()
			args = [
				cmake,
				"-S", str(openblas_source),
				"-B", str(build_dir),
				f"-DCMAKE_BUILD_TYPE={config}",
				f"-DCMAKE_INSTALL_PREFIX={openblas_install}",
				"-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
				"-DBUILD_WITHOUT_LAPACK=OFF",
				"-DC_LAPACK=ON",
				"-DBUILD_WITHOUT_LAPACKE=OFF",
				"-DBUILD_STATIC_LIBS=ON",
				"-DBUILD_SHARED_LIBS=OFF",
				"-DBUILD_TESTING=OFF",
				f"-DDYNAMIC_ARCH={'OFF' if system == 'Windows' else 'ON'}",
				"-DFIXED_LIBNAME=ON",
			]
			if system == "Windows":
				runtime = "MultiThreadedDebugDLL" if config == "Debug" else "MultiThreadedDLL"
				args.append(f"-DCMAKE_MSVC_RUNTIME_LIBRARY={runtime}")
			run(args, f"Configure OpenBLAS {config}")
			run([cmake, "--build", str(build_dir), "--config", config, "--target", "openblas_static", "--parallel", "4"], f"Build OpenBLAS {config}")
			run([cmake, "--install", str(build_dir), "--config", config], f"Install OpenBLAS {config}")
			if not openblas_library.is_file() or not openblas_header.is_file():
				raise RuntimeError(f"OpenBLAS {config} did not install its library and headers")
			(openblas_install / ".gugluu-version").write_text(f"OpenBLAS {OPENBLAS_VERSION}\n", encoding="utf-8")

		if staged_artifacts(
			faiss_base,
			config,
			f"{prefix}faiss{suffix}",
			"faiss/IndexFlat.h",
			f"FAISS {FAISS_VERSION}",
		):
			continue

		build_dir = BUILD / "faiss-int" / config.lower()
		args = [
			cmake,
			"-S", str(faiss_source),
			"-B", str(build_dir),
			f"-DCMAKE_BUILD_TYPE={config}",
			f"-DCMAKE_INSTALL_PREFIX={faiss_install}",
			"-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
			f"-DCMAKE_PREFIX_PATH={openblas_install}",
			f"-DCMAKE_LIBRARY_PATH={openblas_install / 'lib'}",
			f"-DCMAKE_INCLUDE_PATH={openblas_install / 'include' / 'openblas'}",
			"-DBLA_VENDOR=OpenBLAS",
			f"-DBLAS_LIBRARIES={openblas_library}",
			f"-DLAPACK_LIBRARIES={openblas_library}",
			f"-DBLAS_INCLUDE_DIRS={openblas_install / 'include' / 'openblas'}",
			"-DFAISS_ENABLE_GPU=OFF",
			"-DFAISS_ENABLE_PYTHON=OFF",
			"-DFAISS_ENABLE_C_API=OFF",
			"-DFAISS_ENABLE_EXTRAS=OFF",
			"-DFAISS_ENABLE_MKL=OFF",
			"-DFAISS_ENABLE_SVS=OFF",
			"-DFAISS_ENABLE_METAL=OFF",
			"-DFAISS_OPT_LEVEL=generic",
			"-DBUILD_TESTING=OFF",
			"-DBUILD_SHARED_LIBS=OFF",
		]
		if system == "Windows":
			runtime = "MultiThreadedDebugDLL" if config == "Debug" else "MultiThreadedDLL"
			args.append(f"-DCMAKE_MSVC_RUNTIME_LIBRARY={runtime}")
		run(args, f"Configure FAISS {config}")
		run([cmake, "--build", str(build_dir), "--config", config, "--target", "faiss", "--parallel", "4"], f"Build FAISS {config}")
		run([cmake, "--install", str(build_dir), "--config", config], f"Install FAISS {config}")
		library = faiss_install / "lib" / f"{prefix}faiss{suffix}"
		header = faiss_install / "include" / "faiss" / "IndexFlat.h"
		if not library.is_file() or not header.is_file():
			raise RuntimeError(f"FAISS {config} did not install its library and headers")
		(faiss_install / ".gugluu-version").write_text(f"FAISS {FAISS_VERSION}\n", encoding="utf-8")


def premake_archive() -> tuple[str, str]:
	system = platform.system()
	if system == "Windows":
		return f"premake-{PREMAKE_VERSION}-windows.zip", "zip"
	if system == "Linux":
		return f"premake-{PREMAKE_VERSION}-linux.tar.gz", "tgz"
	if system == "Darwin":
		return f"premake-{PREMAKE_VERSION}-macosx.tar.gz", "tgz"
	raise RuntimeError(f"Premake is not available for {system}")


def find_premake() -> list[str]:
	executable_name = "premake5.exe" if os.name == "nt" else "premake5"
	for candidate in (shutil.which("premake5"), shutil.which("premake")):
		if candidate:
			return [candidate]
	local = ROOT / "Vendor" / "Bin" / "Premake" / executable_name
	if not local.is_file():
		filename, _ = premake_archive()
		url = f"https://github.com/premake/premake-core/releases/download/v{PREMAKE_VERSION}/{filename}"
		archive = download(url, DOWNLOADS / filename, PREMAKE_ARCHIVE_SHA256.get(filename))
		staging = BUILD / "premake-staging"
		extract_archive(archive, staging)
		candidates = [
			path for path in staging.rglob("*")
			if path.is_file() and path.name.lower() in ("premake5", "premake5.exe")
		]
		if len(candidates) != 1:
			raise RuntimeError(f"{filename} did not contain exactly one Premake executable")
		local.parent.mkdir(parents=True, exist_ok=True)
		shutil.copy2(candidates[0], local)
		if os.name != "nt":
			local.chmod(local.stat().st_mode | 0o111)
	return [str(local)]


def choose_generator(requested: str | None) -> str:
	if requested:
		return requested
	system = platform.system()
	if system == "Windows":
		choices = [
			("vs2026", "Visual Studio 2026 (.slnx)"),
			("vs2022", "Visual Studio 2022 (.sln)"),
			("gmake2", "GNU Make"),
		]
		default = "vs2026"
	elif system == "Darwin":
		choices = [("xcode4", "Xcode"), ("gmake2", "GNU Make")]
		default = "xcode4"
	else:
		choices = [("gmake2", "GNU Make"), ("codelite", "CodeLite")]
		default = "gmake2"

	print("Choose a project format:")
	for index, (_, label) in enumerate(choices, start=1):
		print(f"  {index}. {label}")
	while True:
		answer = input(f"Select 1-{len(choices)} [default {default}]: ").strip()
		if not answer:
			return default
		if answer.isdigit() and 1 <= int(answer) <= len(choices):
			return choices[int(answer) - 1][0]
		aliases = {choice: choice for choice, _ in choices}
		aliases.update({"vs": "vs2026", "visual studio": "vs2026", "make": "gmake2", "xcode": "xcode4"})
		if answer.lower() in aliases and aliases[answer.lower()] in {choice for choice, _ in choices}:
			return aliases[answer.lower()]
		print("Enter a listed number or generator name.")


def validate_generator(generator: str, system: str) -> None:
	if generator in ("vs2026", "vs2022") and system != "Windows":
		raise RuntimeError("Visual Studio project generation is available only on Windows")
	if generator == "xcode4" and system != "Darwin":
		raise RuntimeError("Xcode project generation is available only on macOS")

def generate_project(generator: str) -> None:
	premake = find_premake()
	# Premake 5.0.0-beta2 has no vs2026 action: generate vs2022, then retarget.
	action = "vs2022" if generator == "vs2026" else generator
	run(premake + [action], f"Generate {generator} project")

	if generator == "vs2026":
		projects = sorted(PROJECT_DIR.glob("*.vcxproj"))
		if not projects:
			raise RuntimeError(f"Premake did not generate a .vcxproj in {PROJECT_DIR}")
		for project in projects:
			text = project.read_text(encoding="utf-8")
			if "<PlatformToolset>v143</PlatformToolset>" not in text:
				raise RuntimeError(f"{project.name} has no v143 toolset to retarget")
			project.write_text(
				text.replace("<PlatformToolset>v143</PlatformToolset>", "<PlatformToolset>v145</PlatformToolset>"),
				encoding="utf-8",
			)
		entries = "".join(
			f'  <Project Path="{escape(p.relative_to(ROOT).as_posix())}" />\n' for p in projects
		)
		solution = ROOT / "Gugluu.slnx"
		solution.write_text(
			'<?xml version="1.0" encoding="utf-8"?>\n'
			'<Solution>\n'
			'  <Configurations>\n'
			'    <BuildType Name="Debug" />\n'
			'    <BuildType Name="Release" />\n'
			'    <Platform Name="x64" />\n'
			'  </Configurations>\n'
			f'{entries}'
			'</Solution>\n',
			encoding="utf-8",
		)
		(ROOT / "Gugluu.sln").unlink(missing_ok=True)  # stale v143 solution from the vs2022 action
		print(f"Generated {solution.name}; open it in Visual Studio 2026.")
	elif generator == "vs2022":
		print("Generated Gugluu.sln in the repository root.")
	else:
		print(f"Generated the {generator} project files in the repository root.")
def main() -> int:
	parser = argparse.ArgumentParser(
		description="Check/install Gugluu dependencies and generate project files (does not build the app)."
	)
	parser.add_argument(
		"generator",
		nargs="?",
		choices=("vs2026", "vs2022", "gmake2", "xcode4", "codelite"),
		help="project format; omit to choose from a prompt",
	)
	args = parser.parse_args()

	try:
		system, arch, platform_name = host_platform()
		generator = choose_generator(args.generator)
		validate_generator(generator, system)
		print("Preparing Gugluu dependencies...", flush=True)
		ensure_runtime_assets(system, arch)
		ensure_spdlog()
		build_faiss_openblas(platform_name, arch, system)
		print("Dependencies are ready.")
		generate_project(generator)
		return 0
	except (OSError, RuntimeError, subprocess.CalledProcessError, urllib.error.URLError) as error:
		print(f"Setup failed: {error}", file=sys.stderr)
		return 1
	except (EOFError, KeyboardInterrupt):
		print("Project generation cancelled.", file=sys.stderr)
		return 1


if __name__ == "__main__":
	raise SystemExit(main())
