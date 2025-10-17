import json
import os
from dataclasses import dataclass
from common_utils import object_from_json, json_remove_comments, find_from_list
from typing import List

"""
tasks.json

{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "script",
            "type": "shell",
            "command": "python3 ${workspaceFolder}/utils/tool/debug/remote_debug.py",
            "args": [],
            "options": {
                "env": {
                    "workspaceFolder": "${workspaceFolder}" // 将workspaceFolder传递给remote_debug.py
                }
            },
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared",
                "clear": false
            },
            "isBackground": true,
            "problemMatcher": {
                "fileLocation": [
                    "relative",
                    "${workspaceFolder}"
                ],
                "pattern": {
                    "regexp": "^(Listening on port)"
                },
                "background": {
                    "activeOnStart": true,
                    "beginsPattern": "Executing",
                    "endsPattern": "^Listening on port" // 终端输出有此内容时证明gdbserver已启动，可执行launch.json任务
                }
            }
        }
    ]
}

launch.json

{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "arm-debug",
            "type": "cppdbg",
            "request": "launch",
            "args": [],
            "cwd": "${workspaceFolder}/utils/build",
            "environment": [
                {
                    "name": "SSH_USER",
                    "value": "root"
                },
                {
                    "name": "SSH_PASSWORD",
                    "value": "root"
                },
                {
                    "name": "SSH_PORT",
                    "value": "22"
                },
                {
                    "name": "GDBSERVER_SH",
                    "value": "${workspaceFolder}/utils/tool/debug/start_gdbserver.sh"
                }
            ],
            "stopAtEntry": false,
            "externalConsole": true,
            "MIMode": "gdb",
            "miDebuggerArgs": "gdb",
            "miDebuggerPath": "/usr/bin/gdb-multiarch",
            "miDebuggerServerAddress": "192.168.123.54:5030",
            "serverLaunchTimeout": 60000,
            "program": "${workspaceFolder}/utils/build/test/main",
            "preLaunchTask": "script",
            "setupCommands": [
                {
                    "description": "为gdb启用整齐打印",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "将反汇编风格设置为Intel",
                    "text": "-gdb-set disassembly-flavor intel",
                    "ignoreFailures": true
                },
                {
                    "description": "STL容器元素显示",
                    "text": "python import sys;sys.path.insert(0, '/usr/share/gcc/python');from libstdcxx.v6.printers import register_libstdcxx_printers;register_libstdcxx_printers(None)",
                    "ignoreFailures": false
                }
            ]
        }
    ]
}
"""

# 从环境变量中获取工作区路径，由tasks.json提供
workspaceFolder = os.getenv("workspaceFolder")
# 定义.vscode文件夹路径
vscode_folder = os.path.join(workspaceFolder, ".vscode")
# 检查.vscode文件夹是否存在
if not os.path.exists(vscode_folder):
    print(f"未找到{vscode_folder}文件夹，请确保你在正确的工作区目录下运行此脚本。")
    exit(1)
# 定义launch.json和tasks.json文件路径
launch_json_path = os.path.join(vscode_folder, "launch.json")
tasks_json_path = os.path.join(vscode_folder, "tasks.json")


@dataclass
class LaunchObject:
    name: str = None
    type: str = None
    request: str = None
    program: str = None
    args: List = None
    cwd: str = None
    environment: List = None
    MIMode: str = None
    miDebuggerArgs: str = None
    miDebuggerPath: str = None
    miDebuggerServerAddress: str = None
    preLaunchTask: str = None
    setupCommands: List = None


class LaunchParser:
    launch_list: List[LaunchObject] = []

    def __init__(self):
        self.config = self.read_launch_json()
        for config in self.config["configurations"]:
            self.launch_list.append(object_from_json(config, LaunchObject))

    def read_launch_json(self):
        # 读取launch.json文件
        try:
            with open(launch_json_path, "r", encoding="utf-8") as file:
                launch_text = file.read()
                launch_text_no_comments = json_remove_comments(launch_text)
                # 将${workspaceFolder}替换为实际的workspaceFolder
                launch_text_no_comments = launch_text_no_comments.replace(
                    "${workspaceFolder}", workspaceFolder
                )
                launch_config = json.loads(launch_text_no_comments)
                return launch_config
        except FileNotFoundError:
            print(f"{launch_json_path} 文件不存在。")
        except Exception as e:
            print(f"读取 {launch_json_path} 时出错: {e}")
        return None

    def get_launch_config(self, name: str) -> LaunchObject:
        for launch in self.launch_list:
            if launch.name == name:
                return launch
        return None

    def get_env_var(self, launch_name: str, env_name: str) -> dict:
        launch_config = self.get_launch_config(launch_name)
        return find_from_list(launch_config.environment, "name", env_name)

    def get_env_value(self, launch_name: str, env_name: str) -> str:
        env_var = self.get_env_var(launch_name, env_name)
        if env_var is None:
            return None
        return env_var["value"]

    def get_env_value_from_object(
        self, launch_object: LaunchObject, env_name: str
    ) -> str:
        env_var = find_from_list(launch_object.environment, "name", env_name)
        if env_var is None:
            return None
        return env_var["value"]


class TaskParser:
    def __init__(self):
        self.config = self.read_tasks_json()

    def read_tasks_json(self):
        # 读取tasks.json文件
        try:
            with open(tasks_json_path, "r", encoding="utf-8") as file:
                tasks_text = file.read()
                tasks_text_no_comments = json_remove_comments(tasks_text)
                # 将${workspaceFolder}替换为实际的workspaceFolder
                launch_text_no_comments = launch_text_no_comments.replace(
                    "${workspaceFolder}", workspaceFolder
                )
                tasks_config = json.loads(tasks_text_no_comments)
                return tasks_config
        except FileNotFoundError:
            print(f"{tasks_json_path} 文件不存在。")
        except Exception as e:
            print(f"读取 {tasks_json_path} 时出错: {e}")
        return None
