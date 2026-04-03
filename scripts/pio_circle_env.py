Import("env")

import os
import platform
import shutil
import subprocess
import tarfile
import urllib.request
import json
from pathlib import Path

from SCons.Script import COMMAND_LINE_TARGETS


def _project_option(name: str, default: str) -> str:
    try:
        value = env.GetProjectOption(name)
    except Exception:
        value = default
    return str(value).strip() if value is not None else default


def _ensure_repo_submodules(project_dir: Path) -> None:
    stdlib_root = project_dir / "third_party" / "circle-stdlib"
    if stdlib_root.exists():
        _run(
            ["git", "submodule", "update", "--init", "--recursive", "third_party/circle-stdlib"],
            project_dir,
        )


def _resolve_circle_paths(project_dir: Path) -> tuple[Path, Path | None]:
    circle_override = os.environ.get("FT_CIRCLE_HOME", "").strip()
    stdlib_override = os.environ.get("FT_CIRCLE_STDLIB_HOME", "").strip()

    candidates = []
    candidates.append(("stdlib", project_dir / "third_party" / "circle-stdlib"))
    candidates.append(("circle", project_dir / "third_party" / "circle"))
    if stdlib_override:
        candidates.append(("stdlib", Path(stdlib_override).expanduser()))
    if circle_override:
        candidates.append(("circle", Path(circle_override).expanduser()))

    for kind, candidate in candidates:
        if not candidate.exists():
            continue

        if kind == "stdlib" or (candidate / "libs" / "circle" / "Rules.mk").is_file():
            circle_home = candidate / "libs" / "circle"
            if (circle_home / "Rules.mk").is_file() and (circle_home / "addon" / "lvgl" / "Makefile").is_file():
                return circle_home.resolve(), candidate.resolve()

        if (candidate / "Rules.mk").is_file() and (candidate / "addon" / "lvgl" / "Makefile").is_file():
            return candidate.resolve(), None

    print("Error: unable to locate the repository-managed Circle dependency tree.")
    print("Expected either `third_party/circle-stdlib` or `third_party/circle` to be initialized.")
    print("Run `git submodule update --init --recursive` and retry.")
    env.Exit(1)


def _ensure_circle_sources(circle_home: Path) -> None:
    lvgl_src = circle_home / "addon" / "lvgl" / "lvgl" / "src"
    if lvgl_src.is_dir():
        return

    print("Circle: initializing missing LVGL addon sources")
    completed = subprocess.run(
        ["git", "submodule", "update", "--init", "addon/lvgl/lvgl"],
        cwd=circle_home,
        check=False,
    )
    if completed.returncode != 0 or not lvgl_src.is_dir():
        print("Error: Circle LVGL sources are missing and could not be initialized.")
        print("Run `git submodule update --init addon/lvgl/lvgl` inside your Circle checkout.")
        env.Exit(1)


def _ensure_stdlib_config(stdlib_home: Path, aarch: str, rasppi: str, tool_prefix: str) -> None:
    config_mk = stdlib_home / "Config.mk"

    needs_configure = not config_mk.is_file()
    if config_mk.is_file():
        config_text = config_mk.read_text(encoding="utf-8", errors="ignore")
        expected_prefix = f"TOOLPREFIX = {tool_prefix}"
        expected_arch = f"-DAARCH={aarch}"
        expected_rasppi_flag = f"-DRASPPI={rasppi}"
        if (
            expected_prefix not in config_text
            or expected_arch not in config_text
            or expected_rasppi_flag not in config_text
        ):
            needs_configure = True

    if not needs_configure:
        return

    if config_mk.is_file():
        _run(["make", "mrproper"], stdlib_home)

    _run(["./configure", "-r", rasppi, "-p", tool_prefix], stdlib_home)


def _kernel_name(aarch: str, rasppi: str) -> str:
    if aarch == "32":
        mapping = {
            "1": "kernel.img",
            "2": "kernel7.img",
            "3": "kernel8-32.img",
            "4": "kernel7l.img",
        }
    else:
        mapping = {
            "3": "kernel8.img",
            "4": "kernel8-rpi4.img",
            "5": "kernel_2712.img",
        }

    if rasppi not in mapping:
        print(f"Error: invalid Circle target combination AARCH={aarch}, RASPPI={rasppi}")
        env.Exit(1)
    return mapping[rasppi]


def _require_tool(prefix: str) -> None:
    compiler = f"{prefix}gcc" if prefix else "gcc"
    if shutil.which(compiler):
        return
    print(f"Error: missing required compiler '{compiler}'")
    print("Install the matching toolchain or override the prefix with FT_CIRCLE_PREFIX32/FT_CIRCLE_PREFIX64.")
    env.Exit(1)


def _select_tool_prefix(explicit_prefix: str, fallbacks: list[str], tool_label: str) -> str:
    if explicit_prefix:
        compiler = f"{explicit_prefix}gcc"
        if shutil.which(compiler):
            return explicit_prefix
        print(f"Circle: requested {tool_label} compiler '{compiler}' not found, trying local fallbacks")

    for prefix in fallbacks:
        compiler = f"{prefix}gcc"
        if shutil.which(compiler):
            print(f"Circle: using local {tool_label} toolchain '{compiler}'")
            return prefix

    return explicit_prefix


def _repo_toolchain_prefix(project_dir: Path, host_arch: str) -> str:
    archive_stems = {
        "x86_64": "arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf",
        "aarch64": "arm-gnu-toolchain-14.3.rel1-aarch64-aarch64-none-elf",
    }
    archive_stem = archive_stems.get(host_arch)
    if archive_stem is None:
        return ""
    return str((project_dir / ".tools" / archive_stem / "bin" / "aarch64-none-elf-").resolve())


def _bootstrap_repo_aarch64_toolchain(project_dir: Path, tool_prefix: str) -> str:
    compiler = f"{tool_prefix}gcc"
    if shutil.which(compiler):
        return tool_prefix

    host_arch = platform.machine().strip().lower()
    local_prefix = _repo_toolchain_prefix(project_dir, host_arch)
    if not local_prefix:
        return tool_prefix

    local_compiler = Path(f"{local_prefix}gcc")
    if local_compiler.is_file():
        print(f"Circle: using repo-local 64-bit toolchain '{local_compiler}'")
        return local_prefix

    downloads_dir = project_dir / ".tools" / "downloads"
    downloads_dir.mkdir(parents=True, exist_ok=True)
    archive_name = f"{local_compiler.parents[1].name}.tar.xz"
    archive_path = downloads_dir / archive_name
    archive_url = (
        "https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/"
        f"{archive_name}"
    )

    print(f"Circle: downloading local 64-bit toolchain from {archive_url}")
    urllib.request.urlretrieve(archive_url, archive_path)

    print(f"Circle: extracting {archive_name}")
    with tarfile.open(archive_path, "r:xz") as archive:
        archive.extractall(project_dir / ".tools")

    if not local_compiler.is_file():
        print(f"Error: expected repo-local compiler '{local_compiler}' after extraction")
        env.Exit(1)

    print(f"Circle: using repo-local 64-bit toolchain '{local_compiler}'")
    return local_prefix


def _run(cmd, cwd: Path) -> None:
    print(f"Circle: {' '.join(cmd)}")
    completed = subprocess.run(cmd, cwd=cwd, check=False)
    if completed.returncode != 0:
        env.Exit(completed.returncode)


def _copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def _sync_lvgl_config(project_dir: Path, circle_home: Path) -> None:
    project_lv_conf = project_dir / "include" / "lv_conf.h"
    circle_lv_conf = circle_home / "addon" / "lvgl" / "lv_conf.h"
    if project_lv_conf.is_file():
        _copy_file(project_lv_conf, circle_lv_conf)


def _apply_patch_if_needed(repo_dir: Path, patch_path: Path) -> None:
    check_cmd = ["git", "apply", "--check", str(patch_path)]
    already_applied_cmd = ["git", "apply", "-R", "--check", str(patch_path)]

    if subprocess.run(check_cmd, cwd=repo_dir, check=False).returncode == 0:
        _run(["git", "apply", str(patch_path)], repo_dir)
        return

    if subprocess.run(already_applied_cmd, cwd=repo_dir, check=False).returncode == 0:
        return

    print(f"Error: could not apply Circle patch '{patch_path.name}' cleanly in {repo_dir}")
    env.Exit(1)


def _apply_circle_patches(project_dir: Path, circle_home: Path) -> None:
    patches_dir = project_dir / "patches" / "circle"
    if not patches_dir.is_dir():
        return

    for patch_path in sorted(patches_dir.glob("*.patch")):
        _apply_patch_if_needed(circle_home, patch_path)


def _copy_boot_payload(circle_home: Path, package_dir: Path, aarch: str) -> None:
    boot_dir = circle_home / "boot"
    overlays_dir = package_dir / "overlays"
    overlays_dir.mkdir(parents=True, exist_ok=True)

    skip_names = {
        ".gitignore",
        "Makefile",
        "README",
        "config32.txt",
        "config64.txt",
    }

    for item in boot_dir.iterdir():
        if item.name in skip_names or item.is_dir():
            continue
        _copy_file(item, package_dir / item.name)

    config_name = "config64.txt" if aarch == "64" else "config32.txt"
    config_src = boot_dir / config_name
    config_dst = package_dir / "config.txt"
    _copy_file(config_src, config_dst)

    config_text = config_dst.read_text(encoding="utf-8", errors="ignore")
    lines = config_text.splitlines()

    def _remove_config_key(entries: list[str], key: str) -> list[str]:
        prefix = f"{key}="
        commented = f"#{prefix}"
        return [entry for entry in entries if not entry.strip().startswith(prefix) and not entry.strip().startswith(commented)]

    lines = _remove_config_key(lines, "enable_uart")
    lines = _remove_config_key(lines, "enable_jtag_gpio")
    lines = _remove_config_key(lines, "gpio")
    insert_at = next((index for index, line in enumerate(lines) if line.startswith("[")), len(lines))
    diag_lines = [
        "",
        "# FluidTouch diagnostics",
        "enable_uart=1",
        "enable_jtag_gpio=1",
        "gpio=22-27=a4",
        "gpio=22-27=pn",
    ]
    lines[insert_at:insert_at] = diag_lines
    config_text = "\n".join(lines) + "\n"
    config_dst.write_text(config_text, encoding="utf-8")

    cmdline_dst = package_dir / "cmdline.txt"
    cmdline_required = [
        "width=800",
        "height=480",
        "logdev=ttyS1",
        "loglevel=2",
        "usbpowerdelay=510",
        "backlight=150",
    ]
    cmdline_text = ""
    if cmdline_dst.exists():
        cmdline_text = cmdline_dst.read_text(encoding="utf-8", errors="ignore").strip()
    cmdline_parts = [part for part in cmdline_text.split() if part]
    cmdline_parts = [part for part in cmdline_parts if part.split("=", 1)[0] not in {
        "width",
        "height",
        "logdev",
        "loglevel",
        "usbpowerdelay",
        "backlight",
    }]
    cmdline_dst.write_text(" ".join(cmdline_required + cmdline_parts) + "\n", encoding="utf-8")

    overlay = boot_dir / "bcm2712d0.dtbo"
    if overlay.exists():
        _copy_file(overlay, overlays_dir / overlay.name)


def _write_build_info(package_dir: Path, circle_home: Path, aarch: str, rasppi: str, kernel: str) -> None:
    info = package_dir / "BUILD_INFO.txt"
    info.write_text(
        "\n".join(
            [
                f"Circle home: {circle_home}",
                f"AARCH: {aarch}",
                f"RASPPI: {rasppi}",
                f"Kernel image: {kernel}",
                "",
                "The bootable SD payload is in this directory.",
                "Copy these files to the FAT boot partition of the target Raspberry Pi.",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def _write_netboot_manifest(
    package_dir: Path,
    circle_home: Path,
    aarch: str,
    rasppi: str,
    kernel: str,
    enabled: bool,
) -> None:
    manifest = {
        "enabled": enabled,
        "circle_home": str(circle_home),
        "aarch": aarch,
        "rasppi": rasppi,
        "kernel": kernel,
        "bootfile": "start4.elf" if rasppi == "4" else "start.elf",
        "boot_dir": str(package_dir),
    }
    _write_json(package_dir / "netboot.json", manifest)


def _read_json(path: Path) -> dict:
    if not path.is_file():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _build_circle() -> None:
    if "clean" in COMMAND_LINE_TARGETS:
        return

    project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
    build_dir = Path(env.subst("$BUILD_DIR")).resolve()
    source_dir_option = _project_option("custom_circle_source_dir", "src/rpi_circle")
    source_dir = (project_dir / source_dir_option).resolve()
    state_dir = build_dir / "circle_state"
    pioenv = env.subst("$PIOENV").strip()
    netboot_enabled = _project_option("custom_circle_netboot", "0").lower() in {"1", "true", "yes", "on"}

    aarch = os.environ.get("FT_CIRCLE_AARCH", _project_option("custom_circle_aarch", "32"))
    rasppi = os.environ.get("FT_CIRCLE_RASPPI", _project_option("custom_circle_rasppi", "4"))
    prefix32 = os.environ.get("FT_CIRCLE_PREFIX32", _project_option("custom_circle_prefix32", "arm-none-eabi-"))
    prefix64 = os.environ.get("FT_CIRCLE_PREFIX64", _project_option("custom_circle_prefix64", "aarch64-none-elf-"))

    prefix32 = _select_tool_prefix(prefix32, ["arm-none-eabi-"], "32-bit")
    if aarch == "64":
        prefix64 = _bootstrap_repo_aarch64_toolchain(project_dir, prefix64)
    else:
        prefix64 = _select_tool_prefix(prefix64, ["aarch64-none-elf-"], "64-bit")

    if aarch not in {"32", "64"}:
        print(f"Error: unsupported FT_CIRCLE_AARCH value '{aarch}'")
        env.Exit(1)

    prefix = prefix64 if aarch == "64" else prefix32
    _require_tool(prefix)

    _ensure_repo_submodules(project_dir)
    circle_home, stdlib_home = _resolve_circle_paths(project_dir)
    _ensure_circle_sources(circle_home)
    _apply_circle_patches(project_dir, circle_home)
    _sync_lvgl_config(project_dir, circle_home)
    kernel = _kernel_name(aarch, rasppi)
    lv_conf_path = project_dir / "include" / "lv_conf.h"
    current_state = {
        "pioenv": pioenv,
        "aarch": aarch,
        "rasppi": rasppi,
        "prefix": prefix,
        "prefix32": prefix32,
        "prefix64": prefix64,
        "circle_home": str(circle_home),
        "stdlib_home": str(stdlib_home) if stdlib_home is not None else "",
        "source_dir": str(source_dir),
        "lv_conf_mtime_ns": lv_conf_path.stat().st_mtime_ns if lv_conf_path.is_file() else 0,
    }
    previous_state = _read_json(state_dir / "build_state.json")
    toolchain_changed = any(
        previous_state.get(key) != current_state.get(key)
        for key in ("aarch", "rasppi", "prefix", "prefix32", "prefix64", "circle_home", "stdlib_home", "source_dir")
    )
    lvgl_changed = toolchain_changed or previous_state.get("lv_conf_mtime_ns") != current_state.get("lv_conf_mtime_ns")

    lvgl_extra_include = (
        f"EXTRAINCLUDE=-I {project_dir / 'include'} "
        f"-I {project_dir / 'src' / 'env' / 'rpi' / 'compat'} "
        f"-I {project_dir / 'src' / 'pc' / 'compat'}"
    )

    make_vars = [
        f"AARCH={aarch}",
        f"RASPPI={rasppi}",
    ]
    if aarch == "64":
        make_vars.append(f"PREFIX64={prefix64}")
    else:
        make_vars.append(f"PREFIX={prefix32}")

    if stdlib_home is not None:
        _ensure_stdlib_config(stdlib_home, aarch, rasppi, prefix)
        _run(["make", *make_vars], stdlib_home)
    else:
        _run(["./makeall", *make_vars], circle_home)
    if lvgl_changed:
        _run(["make", "clean", lvgl_extra_include, *make_vars], circle_home / "addon" / "lvgl")
    _run(["make", lvgl_extra_include, *make_vars], circle_home / "addon" / "lvgl")
    _run(["make", *make_vars], circle_home / "boot")

    if rasppi == "4":
        armstub_target = "armstub64" if aarch == "64" else "armstub"
        _run(["make", armstub_target, *make_vars], circle_home / "boot")

    source_make_vars = [f"CIRCLEHOME={circle_home}", *make_vars]
    if stdlib_home is not None:
        source_make_vars.insert(1, f"CIRCLE_STDLIB_HOME={stdlib_home}")
    source_make_vars.insert(1, f"PIOENV={pioenv}")

    if toolchain_changed:
        _run(["make", "clean", *source_make_vars], source_dir)
    _run(["make", *source_make_vars], source_dir)

    artifact_dir = build_dir / "circle"
    package_dir = artifact_dir / "boot"
    artifact_dir.mkdir(parents=True, exist_ok=True)
    package_dir.mkdir(parents=True, exist_ok=True)

    for suffix in ("img", "elf", "map", "lst"):
        src = source_dir / f"{kernel.rsplit('.', 1)[0]}.{suffix}"
        if src.exists():
            _copy_file(src, artifact_dir / src.name)

    _copy_file(source_dir / kernel, package_dir / kernel)
    _copy_boot_payload(circle_home, package_dir, aarch)
    _write_build_info(package_dir, circle_home, aarch, rasppi, kernel)
    _write_netboot_manifest(package_dir, circle_home, aarch, rasppi, kernel, netboot_enabled)
    _write_json(state_dir / "build_state.json", current_state)

    print(f"Circle target ready: {artifact_dir / kernel}")
    print(f"Circle boot payload ready: {package_dir}")


_build_circle()
