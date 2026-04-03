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


def _run(cmd, env=None):
    try:
        out = subprocess.check_output(cmd, text=True, env=env).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""
    return out


def _pkg_config_candidates():
    candidates = []
    explicit = os.environ.get("FT_RPI_PKG_CONFIG", "").strip()
    if explicit:
        candidates.append(explicit)
    else:
        candidates.extend((f"{TRIPLET}-pkg-config", "pkg-config"))
    return candidates


def _pkg_config_env():
    env_vars = os.environ.copy()
    if SYSROOT:
        env_vars.setdefault("PKG_CONFIG_SYSROOT_DIR", SYSROOT)

    if "PKG_CONFIG_LIBDIR" in env_vars:
        return env_vars

    pc_dirs = []
    if SYSROOT:
        pc_dirs.extend(
            [
                os.path.join(SYSROOT, "usr", "lib", TRIPLET, "pkgconfig"),
                os.path.join(SYSROOT, "usr", "lib", "pkgconfig"),
                os.path.join(SYSROOT, "usr", "share", "pkgconfig"),
            ]
        )
    else:
        pc_dirs.extend(
            [
                os.path.join("/usr", "lib", TRIPLET, "pkgconfig"),
                os.path.join("/usr", "local", "lib", TRIPLET, "pkgconfig"),
                os.path.join("/usr", "share", "pkgconfig"),
            ]
        )

    existing_pc_dirs = [path for path in pc_dirs if os.path.isdir(path)]
    if existing_pc_dirs:
        env_vars["PKG_CONFIG_LIBDIR"] = os.pathsep.join(existing_pc_dirs)
    return env_vars


def _resolve_pkg_config():
    for candidate in _pkg_config_candidates():
        path = shutil.which(candidate)
        if path:
            return path
    return ""


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
    pkg_config = _resolve_pkg_config()
    if not pkg_config:
        print(
            "Error: missing pkg-config for Raspberry Pi cross compile "
            f"(tried: {', '.join(_pkg_config_candidates())})"
        )
        print(
            "Hint: set FT_RPI_PKG_CONFIG, or provide FT_RPI_SDL_CFLAGS and "
            "FT_RPI_SDL_LIBS with target SDL2 paths."
        )
        env.Exit(1)

    pkg_env = _pkg_config_env()
    sdl_cflags = _run([pkg_config, "--cflags", "sdl2"], env=pkg_env)
    sdl_libs = _run([pkg_config, "--libs", "sdl2"], env=pkg_env)
    if not sdl_cflags or not sdl_libs:
        print(
            "Error: unable to resolve target SDL2 flags from pkg-config "
            f"('{os.path.basename(pkg_config)}')"
        )
        print(
            "Hint: install SDL2 dev files for the target sysroot or provide "
            "FT_RPI_SDL_CFLAGS and FT_RPI_SDL_LIBS manually."
        )
        if "PKG_CONFIG_LIBDIR" in pkg_env:
            print(f"Using PKG_CONFIG_LIBDIR={pkg_env['PKG_CONFIG_LIBDIR']}")
        if "PKG_CONFIG_SYSROOT_DIR" in pkg_env:
            print(
                f"Using PKG_CONFIG_SYSROOT_DIR={pkg_env['PKG_CONFIG_SYSROOT_DIR']}"
            )
        env.Exit(1)

    for e in (env, projenv):
        e.MergeFlags(shlex.split(sdl_cflags))
        e.MergeFlags(shlex.split(sdl_libs))

print(f"Configured Raspberry Pi cross compile: triplet={TRIPLET}")
if SYSROOT:
    print(f"Using sysroot: {SYSROOT}")
