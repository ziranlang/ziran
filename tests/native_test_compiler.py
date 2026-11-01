"""Select a direct compiler for tests of Ziran's own native build cache."""
import os
from pathlib import Path
import shutil


def direct_compiler():
    # The default PATH may also contain build-queue launchers. These tests run
    # inside the queue already and require the native driver's own identity.
    compiler = shutil.which(os.environ['CC']) if os.environ.get('CC') else shutil.which('cc', path=os.defpath)
    # CI prepends ccache's aliases to PATH. The fixture must exercise Ziran's
    # own cache/probe counters, independently of an opaque outer cache.
    if compiler and Path(compiler).resolve().name == 'ccache':
        compiler = shutil.which(Path(compiler).name, path=os.defpath)
    assert compiler and Path(compiler).resolve().name != 'ccache', 'a direct C compiler is required'
    return compiler


if __name__ == '__main__':
    print(direct_compiler())
