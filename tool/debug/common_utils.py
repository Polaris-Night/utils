# utils.py

import os
import sys
import logging
import subprocess
import time
import hashlib
import threading
import json
from pathlib import Path
from dataclasses import dataclass, fields, is_dataclass
import shutil
import re


class ElapsedTimer:
    def __init__(self, title=""):
        self.title = title

    def __enter__(self):
        self.start_time = time.time()

    def __exit__(self, exc_type, exc_val, exc_tb):
        end_time = time.time()
        execution_time = end_time - self.start_time
        minutes = int(execution_time // 60)
        seconds = int(execution_time % 60)
        millisecond = int((execution_time - seconds) * 1000)
        print(f"{self.title}耗时：{minutes:02d}:{seconds:02d}.{millisecond:03d}")


class ProgressBar:
    def __init__(self, total, title="", length=40):
        self.total = total
        self.title = title
        self.length = length
        self.start_time = time.time()

    def update(self, progress):
        elapsed_time = time.time() - self.start_time
        remaining_time = (
            (elapsed_time / progress) * (self.total - progress) if progress > 0 else 0
        )
        percent = (progress / self.total) * 100
        bar_length = int(self.length * progress // self.total)
        bar = "█" * bar_length + "-" * (self.length - bar_length)

        sys.stdout.write(
            f"\r{self.title} |{bar}| {percent:.2f}% | Elapsed: {elapsed_time:.2f}s | Remaining: {remaining_time:.2f}s"
        )
        sys.stdout.flush()

    def finish(self):
        sys.stdout.write("\n")
        sys.stdout.flush()


class AtomicCounter:
    def __init__(self):
        self._value = 0
        self._lock = threading.Lock()

    def add_value(self, value):
        with self._lock:
            self._value += value

    def get_value(self):
        with self._lock:
            return self._value


def mkdir_if_not_exists(path: str):
    if not os.path.exists(path):
        os.makedirs(path, exist_ok=True)


def rm_file_or_dir(path):
    if not isinstance(path, Path):
        _path = Path(path)
    else:
        _path = path
    if _path.is_dir():
        shutil.rmtree(_path)
    if _path.is_file():
        _path.unlink()


def run_command(command, capture_output=True, check=True, **kwargs):
    """执行命令，并返回运行结果"""
    try:
        print(f"Execute command: {command}")
        result = subprocess.run(
            command,
            check=check,
            capture_output=capture_output,
            shell=True,
            text=True,
            **kwargs,
        )
        return result
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {command}, Error: {e.stderr}")
        raise e
    except Exception as e:
        print(f"Command failed: {command}, Error: {e}")
        raise e


def md5_file(filepath):
    """计算文件的MD5值"""
    cmd = "command -v md5sum"
    result = run_command(cmd, check=False)
    if result.returncode == 0:
        result = run_command(f"md5sum {filepath}")
        return result.stdout.split(" ")[0].strip()
    else:
        hash_md5 = hashlib.md5()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()


def object_from_json(json_data, data_class):
    """
    从 JSON 字符串或字典对象转换为指定的数据类对象。

    :param json_data: JSON 字符串或字典对象
    :param data_class: 目标数据类
    :return: 数据类对象
    """
    if not is_dataclass(data_class):
        raise ValueError("提供的类必须是数据类")

    # 如果 json_data 是字符串，则先将其转换为字典
    if isinstance(json_data, str):
        json_data = json.loads(json_data)

    # 获取数据类的所有字段
    field_set = {f.name for f in fields(data_class)}

    # 过滤掉字典中不在数据类字段中的键
    filtered_data = {k: v for k, v in json_data.items() if k in field_set}

    # 创建数据类实例
    instance = data_class(**filtered_data)

    return instance


def find_from_list(lst, key, value):
    """
    从列表中根据字典的键值对查找字典。

    :param lst: 包含字典的列表
    :param key: 字典的键（字符串）
    :param value: 键对应的值
    :return: 匹配的字典或 None
    """
    try:
        # 使用 next 函数查找
        return next(item for item in lst if item.get(key) == value)
    except StopIteration:
        # 如果未找到匹配的字典，返回 None
        return None


def json_remove_comments(json_text):
    """移除 JSON 文本中的注释"""
    # 移除单行注释
    single_line_comment_re = re.compile(r"//.*?\n|/\*.*?\*/", re.DOTALL | re.MULTILINE)
    no_single_line_comments = single_line_comment_re.sub("", json_text)

    # 移除多行注释
    multi_line_comment_re = re.compile(r"/\*.*?\*/", re.DOTALL | re.MULTILINE)
    no_comments = multi_line_comment_re.sub("", no_single_line_comments)

    return no_comments
