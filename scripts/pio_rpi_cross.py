Import("env", "projenv")

import os
import shlex
import shutil
import subprocess


# Cross toolchain triplet. Examples:
# - arm-linux-gnueabihf (Raspberry Pi OS 32-bit)
# - aarch64-linux-gnu  (Raspberry Pi OS 64-bit)
TRIPLET = os.environ.get("FT_RPI_TRIPLET", "arm-linux-gnueabihf")
SYSROOT = os.environ.get("FT_RPI_SYSROOT", "").strip()
PKG_CONFIG = os.environ.get("FT_RPI_PKG_CONFIG", f"{TRIPLET}-pkg-config")
SDL_CFLAGS = os.environ.get("FT_RPI_SDL_CFLAGS", "").strip()
SDL_LIBS = os.environ.get("FT_RPI_SDL_LIBS", "").strip()


def _require_tool(name: str) -> str:
    path = shutil.which(name)
    if not path:
        print(f"Error: missing required cross tool '{name}'")
        print(
            "Hint: install the cross toolchain or set FT_RPI_TRIPLET to the one "
            "available on your system."
        )
        env.Exit(1)
    return path


def _run(cmd):
    try:
        out = subprocess.check_output(cmd, text=True).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""
    return out


cc = _require_tool(f"{TRIPLET}-gcc")
cxx = _require_tool(f"{TRIPLET}-g++")
ar = _require_tool(f"{TRIPLET}-ar")
ranlib = _require_tool(f"{TRIPLET}-ranlib")
as_bin = _require_tool(f"{TRIPLET}-as")

# Force PlatformIO native environments to use the cross compiler suite.
for e in (env, projenv):
    e.Replace(CC=cc, CXX=cxx, LINK=cxx, AR=ar, RANLIB=ranlib, AS=as_bin)

if SYSROOT:
    sysroot_flags = [f"--sysroot={SYSROOT}"]
    for e in (env, projenv):
        e.Append(CCFLAGS=sysroot_flags)
        e.Append(LINKFLAGS=sysroot_flags)
    env.Append(CPPDEFINES=[("FT_RPI_SYSROOT", f'\"{SYSROOT}\"')])

if SDL_CFLAGS or SDL_LIBS:
    if SDL_CFLAGS:
        for e in (env, projenv):
            e.MergeFlags(shlex.split(SDL_CFLAGS))
    if SDL_LIBS:
        for e in (env, projenv):
            e.MergeFlags(shlex.split(SDL_LIBS))
else:
    pkg_config = shutil.which(PKG_CONFIG)
    if not pkg_config:
        print(f"Error: missing target pkg-config '{PKG_CONFIG}'")
        print(
            "Hint: set FT_RPI_PKG_CONFIG, or provide FT_RPI_SDL_CFLAGS and "
            "FT_RPI_SDL_LIBS with target SDL2 paths."
        )
        env.Exit(1)

    sdl_cflags = _run([pkg_config, "--cflags", "sdl2"])
    sdl_libs = _run([pkg_config, "--libs", "sdl2"])
    if not sdl_cflags or not sdl_libs:
        print("Error: unable to resolve SDL2 flags from target pkg-config")
        print(
            "Hint: install SDL2 dev files for the target sysroot or provide "
            "FT_RPI_SDL_CFLAGS and FT_RPI_SDL_LIBS manually."
        )
        env.Exit(1)

    for e in (env, projenv):
        e.MergeFlags(shlex.split(sdl_cflags))
        e.MergeFlags(shlex.split(sdl_libs))

print(f"Configured Raspberry Pi cross compile: triplet={TRIPLET}")
if SYSROOT:
    print(f"Using sysroot: {SYSROOT}")
