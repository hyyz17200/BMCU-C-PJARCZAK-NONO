"""Stage and publish the build matrix under this checkout's dist/firmwares only."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[1]


def linked(path):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    return stat.S_ISLNK(info.st_mode) or bool(
        getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0))


def output_root():
    dist = ROOT / "dist"
    if linked(dist):
        raise ValueError("dist must not be a symlink or junction")
    dist.mkdir(exist_ok=True)
    if dist.resolve().parent != ROOT.resolve():
        raise ValueError("dist must stay inside this checkout")
    return dist.resolve()


def checked_stage(value):
    dist = output_root()
    stage = Path(value)
    if not stage.is_absolute():
        stage = ROOT / stage
    if (linked(stage) or stage.parent.resolve() != dist or
            not re.fullmatch(r"\.firmwares-[0-9a-f]{32}", stage.name)):
        raise ValueError("not a staging directory owned by this build")
    return stage


def begin():
    stage = output_root() / (".firmwares-" + uuid.uuid4().hex)
    stage.mkdir()
    return stage


def manifest(stage):
    entries = []
    for directory, dirs, files in os.walk(stage, followlinks=False):
        for name in dirs + files:
            if linked(Path(directory) / name):
                raise ValueError("staged output must not contain links")
        for name in files:
            path = Path(directory) / name
            rel = path.relative_to(stage).as_posix()
            if rel == "manifest.txt":
                continue
            data = path.read_bytes()
            entries.append((rel, hashlib.sha256(data).hexdigest(),
                            f"{zlib.crc32(data) & 0xffffffff:08X}", len(data)))
    text = "# format: SHA256_HEX CRC32_HEX SIZE_BYTES REL_PATH\n"
    text += "".join(f"{sha} {crc} {size} {rel}\n" for rel, sha, crc, size in sorted(entries))
    (stage / "manifest.txt").write_bytes(text.encode("utf-8"))


def publish_locked(value):
    stage = checked_stage(value)
    dist = output_root()
    target = dist / "firmwares"
    if linked(target) or (target.exists() and not target.is_dir()):
        raise ValueError("dist/firmwares must be a normal directory")
    if not stage.is_dir():
        raise ValueError("staged output does not exist")
    manifest(stage)
    backup = dist / (".firmwares-backup-" + uuid.uuid4().hex)
    had_output = target.exists()
    if had_output:
        target.rename(backup)
    try:
        stage.rename(target)
    except BaseException:
        if had_output:
            backup.rename(target)
        raise
    if had_output:
        # Both paths were resolved inside dist before any rename/delete above.
        try:
            shutil.rmtree(backup)
        except OSError as error:
            print(f"Output published; previous output retained at {backup}: {error}", file=sys.stderr)


def publish(value):
    # Concurrent builders may stage independently, but never swap the output together.
    lock = output_root() / ".firmwares-publish.lock"
    with lock.open("x"):
        pass
    try:
        publish_locked(value)
    finally:
        lock.unlink()


def cleanup(value):
    stage = checked_stage(value)
    if stage.exists():
        shutil.rmtree(stage)


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "begin":
        # No newline: command substitution also works with Windows Python in Git Bash.
        sys.stdout.write(begin().relative_to(ROOT).as_posix())
    elif len(sys.argv) == 3 and sys.argv[1] in ("publish", "cleanup"):
        {"publish": publish, "cleanup": cleanup}[sys.argv[1]](sys.argv[2])
    else:
        raise SystemExit("Usage: firmware_output.py begin | publish STAGE | cleanup STAGE")


if __name__ == "__main__":
    main()
