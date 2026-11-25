"""Compile and cache native C++17 build helpers in a private per-user directory."""
import errno
import hashlib
import os
import platform
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

_COMPILER_CACHE = {}
_CHMOD_IGNORED = tuple(
    getattr(errno, name)
    for name in ('EPERM', 'EACCES', 'ENOTSUP', 'EOPNOTSUPP', 'EINVAL')
    if hasattr(errno, name)
)


def _chmod_best_effort(path, mode):
    try:
        path.chmod(mode)
    except (PermissionError, NotImplementedError):
        pass
    except OSError as exc:
        if exc.errno not in _CHMOD_IGNORED:
            raise


def find_compiler():
    cc = shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')
    if not cc:
        raise SystemExit('C++17 compiler required for native navigation helpers')
    return cc


def compiler_identity(cc):
    resolved = str(Path(cc).resolve())
    cached = _COMPILER_CACHE.get(resolved)
    if cached is not None:
        return cached
    version = ''
    try:
        out = subprocess.check_output(
            [cc, '--version'], stderr=subprocess.STDOUT, text=True, timeout=10
        )
        version = out.splitlines()[0].strip() if out else ''
    except (OSError, subprocess.SubprocessError):
        version = resolved
    ident = f'{resolved}|{version}'
    _COMPILER_CACHE[resolved] = ident
    return ident


def cache_key(src, flags, cc):
    h = hashlib.sha256()
    h.update(Path(src).read_bytes())
    h.update(b'\0' + sys.platform.encode() + b'\0' + platform.machine().encode())
    h.update(b'\0' + compiler_identity(cc).encode())
    h.update(b'\0' + '\0'.join(flags).encode())
    return h.hexdigest()[:16]


def private_cache_dir(base_dir=None, fallback_dir=None):
    """Return a private 0700 directory owned by the current user, rejecting symlinks."""
    uid = os.getuid() if hasattr(os, 'getuid') else None
    suffix = str(uid) if uid is not None else os.environ.get('USERNAME', 'user')
    root = Path(base_dir) if base_dir is not None else Path(tempfile.gettempdir())
    candidate = root / f'clioplay-helpers-{suffix}'

    def usable(directory):
        try:
            st = directory.lstat()
        except FileNotFoundError:
            try:
                os.mkdir(directory, 0o700)
            except FileExistsError:
                pass
            except OSError:
                return False
            try:
                st = directory.lstat()
            except OSError:
                return False
        except OSError:
            return False
        if stat.S_ISLNK(st.st_mode) or not stat.S_ISDIR(st.st_mode):
            return False
        if uid is not None and st.st_uid != uid:
            return False
        if uid is not None and (st.st_mode & 0o077):
            _chmod_best_effort(directory, 0o700)
        return os.access(directory, os.R_OK | os.W_OK | os.X_OK)

    if usable(candidate):
        return candidate
    if fallback_dir is not None:
        fb = Path(fallback_dir)
        fb.mkdir(parents=True, exist_ok=True)
        return fb
    raise RuntimeError(f'Cannot create private helper cache directory: {candidate}')


def _compile_into(target_dir, exe_name, src, flags, cc, log=None):
    target_dir = Path(target_dir)
    target_dir.mkdir(parents=True, exist_ok=True)
    exe = target_dir / exe_name
    try:
        st = exe.lstat()
        if stat.S_ISLNK(st.st_mode):
            exe.unlink()
        elif stat.S_ISREG(st.st_mode) and st.st_size > 0:
            return exe
    except FileNotFoundError:
        pass
    handle, temporary = tempfile.mkstemp(
        dir=str(target_dir), prefix=exe_name + '.', suffix='.tmp'
    )
    os.close(handle)
    tmp = Path(temporary)
    try:
        cmd = [cc, *flags, str(src), '-o', str(tmp)]
        if log:
            log('+', ' '.join(cmd))
        subprocess.run(cmd, check=True)
        _chmod_best_effort(tmp, 0o755)
        os.replace(tmp, exe)
    finally:
        tmp.unlink(missing_ok=True)
    return exe


def ensure_compiled(name, src, flags, cc=None, cache_base=None, fallback_dir=None, log=None):
    """Compile src into the private helper cache (reusing across regions) and return its path."""
    cc = cc or find_compiler()
    src = Path(src)
    exe_name = f'{name}-{cache_key(src, flags, cc)}'
    cache_dir = private_cache_dir(base_dir=cache_base, fallback_dir=fallback_dir)
    return _compile_into(cache_dir, exe_name, src, flags, cc, log=log)


def run_compiled(name, src, flags, args, cc=None, cache_base=None, fallback_dir=None, log=None):
    """Run a cached helper binary, falling back to fallback_dir if cache_dir is mounted noexec."""
    cc = cc or find_compiler()
    src = Path(src)
    exe = ensure_compiled(
        name, src, flags, cc=cc, cache_base=cache_base, fallback_dir=fallback_dir, log=log
    )
    cmd = [str(exe), *map(str, args)]
    if log:
        log('+', ' '.join(cmd))
    try:
        return subprocess.run(cmd, check=True)
    except PermissionError:
        if fallback_dir is None or exe.parent.resolve() == Path(fallback_dir).resolve():
            raise
        # Handle systems where /tmp is mounted noexec by compiling into a cleaned temp dir under fallback_dir.
        with tempfile.TemporaryDirectory(dir=str(fallback_dir), prefix=f'.{name}-') as td:
            fb_exe = _compile_into(td, f'{name}-{cache_key(src, flags, cc)}', src, flags, cc, log=log)
            fb_cmd = [str(fb_exe), *map(str, args)]
            if log:
                log('+', ' '.join(fb_cmd))
            return subprocess.run(fb_cmd, check=True)
