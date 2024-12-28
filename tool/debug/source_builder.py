import os
import abc
from dataclasses import dataclass
from common_utils import run_command, mkdir_if_not_exists, rm_file_or_dir
from typing import Literal


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


class BuildHelper:
    def __new__(self):
        raise NotImplementedError("class BuildHelper can not be instantiated")

    @staticmethod
    def build_source(builder: AbstractBuilder):
        if not isinstance(builder, AbstractBuilder):
            raise TypeError("builder must be a subclass of AbstractBuilder")
        builder.clean()
        builder.configure()
        builder.build()

    @staticmethod
    def build_and_install(builder: AbstractBuilder):
        BuildHelper.build_source(builder)
        builder.install()


@dataclass
class CMakeBuildParam:
    source_path: str = None
    build_path: str = None
    install_path: str = None
    build_type: Literal["Debug", "Release", "RelWithDebInfo", "MinSizeRel"] = "Release"
    build_target: str | list[str] = "all"  # e.g. ["app1", "app2"] or "all"
    cmake_cache_args: list[str] = (
        None  # needless "-D", e.g. ["CMAKE_CXX_STANDARD=c++17"]
    )
    custom_args: list[str] = None  # e.g. ["-G Ninja"]
    build_jobs: int = max(1, os.cpu_count() - 2)


class CMakeBuilder(AbstractBuilder):
    def __init__(self, param: CMakeBuildParam):
        if not param.source_path or not param.build_path:
            raise ValueError("source_path or build_path is empty")
        self.param = param

    def configure(self):
        mkdir_if_not_exists(self.param.build_path)
        cmake_cmd = [
            "cmake",
            "-S",
            self.param.source_path,
            "-B",
            self.param.build_path,
            f"-DCMAKE_BUILD_TYPE={self.param.build_type}",
        ]
        if self.param.install_path:
            cmake_cmd.append(f"-DCMAKE_INSTALL_PREFIX={self.param.install_path}")
        if self.param.cmake_cache_args:
            cmake_cmd.extend(f"-D{arg}" for arg in self.param.cmake_cache_args)
        if self.param.custom_args:
            cmake_cmd.extend(self.param.custom_args)
        run_command(" ".join(cmake_cmd), capture_output=False)

    def build(self):
        target = (
            " ".join(self.param.build_target)
            if isinstance(self.param.build_target, list)
            else self.param.build_target
        )
        build_cmd = f"cmake --build {self.param.build_path} --target {target} -j{self.param.build_jobs}"
        run_command(build_cmd, capture_output=False)

    def install(self):
        install_cmd = f"cmake --install {self.param.build_path}"
        run_command(install_cmd, capture_output=False)

    def clean(self):
        rm_file_or_dir(os.path.join(self.param.build_path, "CMakeCache.txt"))
