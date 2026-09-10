#!/usr/bin/env python3
"""Run the native clang-tidy contract, including when a shared header changes."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
os.chdir(root)
tidy = os.environ.get("CLANG_TIDY") or shutil.which("clang-tidy")
if not tidy and Path("/opt/homebrew/opt/llvm/bin/clang-tidy").exists():
    tidy = "/opt/homebrew/opt/llvm/bin/clang-tidy"
if not tidy:
    sys.exit("Install clang-tidy or set CLANG_TIDY before committing C++ changes.")
build = root / "build-lint"
subprocess.run(["cmake", "-S", ".", "-B", str(build), "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], check=True)
subprocess.run(["cmake", "--build", str(build), "--target", "format-check"], check=True)
commands = json.loads((build / "compile_commands.json").read_text())
sources = sorted({entry["file"] for entry in commands if entry["file"].endswith(".cpp")})
arguments = [tidy, "-p", str(build)]
if sys.platform == "darwin":
    sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    arguments += ["--extra-arg=-isysroot", "--extra-arg=" + sdk,
                  "--extra-arg=-isystem" + sdk + "/usr/include/c++/v1"]
subprocess.run(arguments + sources, check=True)
