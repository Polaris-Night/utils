import os
import abc
from common_utils import run_command, mkdir_is_not_exists, rm_file_or_dir


class AbstractBuilder(abc.ABC):
    @abc.abstractmethod
    def configure(self):
        pass

    @abc.abstractmethod
    def build(self):
        pass

    def install(self):
        pass

    def clean(self):
        pass


class CmakeBuilder(AbstractBuilder):
    def __init__(self, source_dir, build_dir, build_jobs=4, cmake_args=None):
        self.source_dir = source_dir
        self.build_dir = build_dir
        self.build_jobs = build_jobs
        self.cmake_args = cmake_args

    def configure(self):
        mkdir_is_not_exists(self.build_dir)
        cmake_cmd = [
            "cmake",
            "-S",
            self.source_dir,
            "-B",
            self.build_dir,
        ]
        if self.cmake_args:
            cmake_cmd.extend(f"-D{arg}" for arg in self.cmake_args)
        run_command(" ".join(cmake_cmd))

    def build(self):
        build_cmd = f"cmake --build {self.build_dir} -j{self.build_jobs}"
        run_command(build_cmd)

    def install(self):
        install_cmd = f"cmake --install {self.build_dir}"
        run_command(install_cmd)

    def clean(self):
        rm_file_or_dir(os.path.join(self.build_dir, "CMakeCache.txt"))


def build_source(builder):
    builder.clean()
    builder.configure()
    builder.build()


def build_and_install(builder):
    build_source(builder)
    builder.install()
