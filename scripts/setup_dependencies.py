#!/usr/bin/env python3
"""Verify or download Gugluu's pinned model and ONNX Runtime dependencies."""

import argparse
import hashlib
import json
import os
import shutil
import stat
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath


c_RepositoryRoot = Path(__file__).resolve().parents[1]
c_ModelDirectory = c_RepositoryRoot / "Gugluu" / "Vendor" / "Model" / "all-MiniLM-L6-v2"
c_OnnxDirectory = c_RepositoryRoot / "Gugluu" / "Vendor" / "ONNX"
c_OnnxVersion = "1.31.1"
c_OnnxArchiveUrl = (
	"https://github.com/microsoft/onnxruntime/releases/download/"
	f"v{c_OnnxVersion}/onnxruntime-win-x64-{c_OnnxVersion}.zip"
)
c_OnnxArchiveSha256 = "5869e26bb25314375f2d5ef1877e3ea06b1543d6c710e6e4a1ec2f7177ab4b2c"
c_OnnxArchiveDirectory = f"onnxruntime-win-x64-{c_OnnxVersion}"
c_OnnxRequiredFiles = (
	"include/onnxruntime_c_api.h",
	"include/onnxruntime_cxx_api.h",
	"include/onnxruntime_cxx_inline.h",
	"lib/onnxruntime.dll",
	"lib/onnxruntime.lib",
	"lib/onnxruntime_providers_shared.dll",
)
c_ModelRevision = "1110a243fdf4706b3f48f1d95db1a4f5529b4d41"
c_ModelBaseUrl = (
	"https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/resolve/"
	f"{c_ModelRevision}/"
)
c_ModelFiles = {
	"model.onnx": "6fd5d72fe4589f189f8ebc006442dbb529bb7ce38f8082112682524616046452",
	"tokenizer.json": "be50c3628f2bf5bb5e3a7f17b1f74611b2561a3a27eeab05e5aa30f411572037",
	"tokenizer_config.json": "acb92769e8195aabd29b7b2137a9e6d6e25c476a4f15aa4355c233426c61576b",
	"special_tokens_map.json": "303df45a03609e4ead04bc3dc1536d0ab19b5358db685b6f3da123d05ec200e3",
	"vocab.txt": "07eced375cec144d27c900241f3e339478dec958f92fddbc551f295c992038a3",
}
c_ModelRemotePaths = {
	"model.onnx": "onnx/model.onnx",
	"tokenizer.json": "tokenizer.json",
	"tokenizer_config.json": "tokenizer_config.json",
	"special_tokens_map.json": "special_tokens_map.json",
	"vocab.txt": "vocab.txt",
}
c_DownloadChunkSize = 1024 * 1024


def GetSha256(a_Path):
	c_Hash = hashlib.sha256()
	with a_Path.open("rb") as c_File:
		for c_Chunk in iter(lambda: c_File.read(c_DownloadChunkSize), b""):
			c_Hash.update(c_Chunk)
	return c_Hash.hexdigest()


def DownloadVerifiedFile(a_Url, a_Destination, a_ExpectedSha256):
	a_Destination.parent.mkdir(parents=True, exist_ok=True)
	c_TemporaryPath = None

	try:
		with tempfile.NamedTemporaryFile(
			mode="wb",
			prefix=f"{a_Destination.name}.",
			suffix=".part",
			dir=a_Destination.parent,
			delete=False,
		) as c_TemporaryFile:
			c_TemporaryPath = Path(c_TemporaryFile.name)
			c_Hash = hashlib.sha256()
			c_Request = urllib.request.Request(
				a_Url,
				headers={"User-Agent": "Gugluu-dependency-setup/1.0"},
			)
			with urllib.request.urlopen(c_Request, timeout=60) as c_Response:
				while True:
					c_Chunk = c_Response.read(c_DownloadChunkSize)
					if not c_Chunk:
						break
					c_TemporaryFile.write(c_Chunk)
					c_Hash.update(c_Chunk)

		if c_Hash.hexdigest() != a_ExpectedSha256:
			raise ValueError(f"SHA-256 verification failed for {a_Destination.name}.")

		os.replace(c_TemporaryPath, a_Destination)
		c_TemporaryPath = None
	finally:
		if c_TemporaryPath is not None:
			c_TemporaryPath.unlink(missing_ok=True)


def IsModelAssetValid(a_Name):
	c_Path = c_ModelDirectory / a_Name
	if not c_Path.is_file() or c_Path.stat().st_size == 0:
		return False

	if a_Name == "model.onnx":
		return GetSha256(c_Path) == c_ModelFiles[a_Name]

	if a_Name == "tokenizer.json":
		try:
			with c_Path.open("r", encoding="utf-8") as c_File:
				c_Tokenizer = json.load(c_File)
			if not isinstance(c_Tokenizer, dict):
				return False
			c_Model = c_Tokenizer.get("model")
			c_Vocabulary = c_Model.get("vocab") if isinstance(c_Model, dict) else None
			return (
				isinstance(c_Vocabulary, dict)
				and c_Model.get("type") == "WordPiece"
				and len(c_Vocabulary) == 30522
			)
		except (OSError, UnicodeError, json.JSONDecodeError):
			return False

	return True


def EnsureModelFiles(a_CheckOnly):
	c_MissingFiles = [
		c_Name for c_Name in c_ModelFiles if not IsModelAssetValid(c_Name)
	]
	if a_CheckOnly:
		if c_MissingFiles:
			raise FileNotFoundError(
				"Model assets are missing or invalid: " + ", ".join(c_MissingFiles)
			)
		print("Model assets are installed and verified.")
		return

	for c_Name in c_MissingFiles:
		print(f"Downloading model asset: {c_Name}")
		DownloadVerifiedFile(
			c_ModelBaseUrl + c_ModelRemotePaths[c_Name],
			c_ModelDirectory / c_Name,
			c_ModelFiles[c_Name],
		)

	print("Model assets are installed and verified.")


def IsOnnxInstallValid():
	c_VersionPath = c_OnnxDirectory / "VERSION_NUMBER"
	if not c_VersionPath.is_file():
		return False

	if c_VersionPath.read_text(encoding="utf-8").strip() != c_OnnxVersion:
		return False

	return all(
		(c_OnnxDirectory / c_Name).is_file()
		and (c_OnnxDirectory / c_Name).stat().st_size > 0
		for c_Name in c_OnnxRequiredFiles
	)


def ExtractOnnxArchive(a_ArchivePath, a_StagingDirectory):
	c_ArchivePrefix = c_OnnxArchiveDirectory + "/"
	c_ExpectedFiles = set(c_OnnxRequiredFiles)
	c_ExtractedFiles = set()

	with zipfile.ZipFile(a_ArchivePath) as c_Archive:
		for c_Info in c_Archive.infolist():
			c_Name = c_Info.filename
			if not c_Name.startswith(c_ArchivePrefix):
				continue

			c_RelativeName = c_Name[len(c_ArchivePrefix):]
			if not c_RelativeName and c_Info.is_dir():
				continue

			c_RelativePath = PurePosixPath(c_RelativeName)
			if (
				not c_RelativeName
				or "\\" in c_RelativeName
				or c_RelativePath.is_absolute()
				or ".." in c_RelativePath.parts
			):
				raise ValueError(f"Unsafe path in ONNX Runtime archive: {c_Name}")

			if not c_RelativePath.parts or c_RelativePath.parts[0] not in ("include", "lib"):
				continue

			if stat.S_ISLNK(c_Info.external_attr >> 16):
				raise ValueError(f"Symbolic link in ONNX Runtime archive: {c_Name}")

			c_Destination = a_StagingDirectory.joinpath(*c_RelativePath.parts)
			if c_Info.is_dir():
				c_Destination.mkdir(parents=True, exist_ok=True)
				continue

			c_Destination.parent.mkdir(parents=True, exist_ok=True)
			with c_Archive.open(c_Info) as c_Source, c_Destination.open("wb") as c_Output:
				shutil.copyfileobj(c_Source, c_Output, c_DownloadChunkSize)
			if c_RelativeName in c_ExpectedFiles:
				c_ExtractedFiles.add(c_RelativeName)

	c_MissingFiles = c_ExpectedFiles - c_ExtractedFiles
	if c_MissingFiles:
		raise ValueError(
			"ONNX Runtime archive is missing required files: "
			+ ", ".join(sorted(c_MissingFiles))
		)


def InstallOnnxRuntime():
	c_OnnxDirectory.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="setup-", dir=c_OnnxDirectory) as c_TempName:
		c_TempDirectory = Path(c_TempName)
		c_ArchivePath = c_TempDirectory / "onnxruntime.zip"
		print(f"Downloading ONNX Runtime {c_OnnxVersion} for Windows x64.")
		DownloadVerifiedFile(c_OnnxArchiveUrl, c_ArchivePath, c_OnnxArchiveSha256)

		c_StagingDirectory = c_TempDirectory / "extracted"
		c_StagingDirectory.mkdir()
		ExtractOnnxArchive(c_ArchivePath, c_StagingDirectory)

		for c_Subdirectory in ("include", "lib"):
			c_SourceDirectory = c_StagingDirectory / c_Subdirectory
			for c_SourcePath in c_SourceDirectory.rglob("*"):
				if c_SourcePath.is_file():
					c_RelativePath = c_SourcePath.relative_to(c_StagingDirectory)
					c_Destination = c_OnnxDirectory / c_RelativePath
					c_Destination.parent.mkdir(parents=True, exist_ok=True)
					shutil.copy2(c_SourcePath, c_Destination)

	(c_OnnxDirectory / "VERSION_NUMBER").write_text(
		c_OnnxVersion + "\n", encoding="utf-8"
	)
	if not IsOnnxInstallValid():
		raise RuntimeError("ONNX Runtime installation did not pass validation.")
	print(f"ONNX Runtime {c_OnnxVersion} is installed and verified.")


def EnsureOnnxRuntime(a_CheckOnly):
	if IsOnnxInstallValid():
		print(f"ONNX Runtime {c_OnnxVersion} is installed and verified.")
		return
	if a_CheckOnly:
		raise FileNotFoundError(
			f"ONNX Runtime {c_OnnxVersion} is missing or incomplete in {c_OnnxDirectory}."
		)
	InstallOnnxRuntime()


def Main():
	if sys.version_info < (3, 8):
		print("Python 3.8 or newer is required.", file=sys.stderr)
		return 1

	c_Parser = argparse.ArgumentParser(description=__doc__)
	c_Parser.add_argument(
		"--check",
		action="store_true",
		help="verify dependencies without downloading or changing files",
	)
	c_Arguments = c_Parser.parse_args()

	try:
		EnsureModelFiles(c_Arguments.check)
		EnsureOnnxRuntime(c_Arguments.check)
	except (OSError, urllib.error.URLError, zipfile.BadZipFile, ValueError, RuntimeError) as c_Error:
		print(f"Dependency setup failed: {c_Error}", file=sys.stderr)
		return 1

	return 0


if __name__ == "__main__":
	raise SystemExit(Main())
