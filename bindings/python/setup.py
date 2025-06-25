import os
import shutil
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext
from setuptools.command.sdist import sdist as _sdist

base_dir = os.path.abspath(os.path.dirname(__file__))
libsrc_dir = os.path.join(base_dir, "libsrc")

# External source locations
external_dirs = [
    os.path.abspath(os.path.join(base_dir, "../../lib/libsdm")),
    os.path.abspath(os.path.join(base_dir, "../../lib/libsdm/janus")),
    os.path.abspath(os.path.join(base_dir, "../../lib/libstream")),
]

def copy_sources():
    for ext_dir in external_dirs:
        if not os.path.isdir(ext_dir):
            continue
        rel_path = os.path.relpath(ext_dir, os.path.join(base_dir, "../.."))
        dst_dir = os.path.join(libsrc_dir, rel_path)
        os.makedirs(dst_dir, exist_ok=True)
        print(f"y. {rel_path}, {dst_dir}")

        for fname in os.listdir(ext_dir):
            if fname.endswith((".c", ".h")):
                shutil.copy2(os.path.join(ext_dir, fname), os.path.join(dst_dir, fname))

class CustomBuildExt(_build_ext):
    def run(self):
        copy_sources()
        super().run()


from setuptools.command.sdist import sdist as _sdist
import shutil

class CustomSdist(_sdist):
    def make_release_tree(self, base_dir, files):
        print("→ Copying .c and .h files to libsrc/")
        copied_files = []

        for ext_dir in external_dirs:
            if not os.path.isdir(ext_dir):
                continue
            rel_path = os.path.relpath(ext_dir, os.path.join(base_dir, "../.."))
            dst_dir = os.path.join(libsrc_dir, rel_path)
            os.makedirs(dst_dir, exist_ok=True)

            for fname in os.listdir(ext_dir):
                if fname.endswith((".c", ".h", ".def")):
                    src = os.path.join(ext_dir, fname)
                    dst = os.path.join(dst_dir, fname)
                    shutil.copy2(src, dst)
                    copied_files.append(os.path.relpath(dst, start=os.path.dirname(__file__)))

        # Add them to the release tree
        files += copied_files
        super().make_release_tree(base_dir, files)

# Prepare include/source paths
src_dirs = [
    "lib/libsdm",
    "lib/libsdm/janus",
    "lib/libstream",
]

swig_opts = ["-Wall", "-globals", "var"]

for inc in src_dirs:
    swig_opts.append(f"-I{os.path.join(base_dir, inc)}")
    swig_opts.append(f"-I{os.path.join(libsrc_dir, inc)}")

# Base sources
sources = ["sdm.i"]

# Add discovered .c files
for src_dir in src_dirs:
    if os.path.exists(src_dir):
        for fname in os.listdir(src_dir):
            if fname.endswith(".c"):
                sources.append(os.path.join(src_dir, fname))

sdm_module = Extension(
    name="_sdm",
    sources=sources,
    include_dirs=src_dirs,
    swig_opts=swig_opts,
    extra_compile_args = ["-DLOGGER_ENABLED", "-D_GNU_SOURCE"],
)

setup(
    name="sdm",
    version="0.0.1",
    description="SWIG wrapper for SDM",
    py_modules=["sdm", "sdmapi"],
    ext_modules=[sdm_module],
    cmdclass={
        "sdist": CustomSdist,
    },
)

