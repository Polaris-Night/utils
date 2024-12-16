import os
from ssh_processer import SSHProcessor
from vscode_parser import *


@dataclass
class DebugArgs:
    remote_host: str = None  # 远程ip
    ssh_port: int = None  # ssh端口
    remote_user: str = None  # ssh用户名
    remote_password: str = None  # ssh密码
    gdbserver_port: int = None  # gdbserver端口

    gdbserver_sh: str = None  # gdbserver启动脚本
    app_name: str = None  # 调试app名称
    app_dir: str = None  # app目录


class Debugger:

    def __init__(self):
        parser = LaunchParser()
        self.arm_debug = parser.get_launch_config("arm-debug")
        self.debug_args = DebugArgs()

        # remote args
        host, port = self.arm_debug.miDebuggerServerAddress.split(":")
        self.debug_args.remote_host = host
        self.debug_args.gdbserver_port = int(port)
        self.debug_args.ssh_port = int(
            parser.get_env_value_from_object(self.arm_debug, "SSH_PORT")
        )
        self.debug_args.remote_user = parser.get_env_value_from_object(
            self.arm_debug, "SSH_USER"
        )
        self.debug_args.remote_password = parser.get_env_value_from_object(
            self.arm_debug, "SSH_PASSWORD"
        )
        self.debug_args.gdbserver_sh = parser.get_env_value_from_object(
            self.arm_debug, "GDBSERVER_SH"
        )
        self.debug_args.app_name = os.path.basename(self.arm_debug.program)

        # ssh client
        self.ssh_client = SSHProcessor(
            self.debug_args.remote_host,
            self.debug_args.ssh_port,
            self.debug_args.remote_user,
            self.debug_args.remote_password,
        )

    def build_app(self):
        print("Building...")

    def connect_ssh(self):
        print("Connecting...")
        self.ssh_client.connect()

    def diff_and_copy(self):
        print("Transferring...")

    def start_gdbserver(self):
        print(f"Executing...")
        gdbserver_sh = os.path.join(
            self.debug_args.app_dir,
            os.path.basename(self.debug_args.gdbserver_sh),
        )
        command = "&& ".join(
            [
                f"cd {self.debug_args.app_dir}",
                f"chmod +x {gdbserver_sh}",
                f"export GDBSERVER_PORT={self.debug_args.gdbserver_port}"
                f"export APP_NAME={self.debug_args.app_name}"
                f"source ./{os.path.basename(self.debug_args.gdbserver_sh)}",  # 使用source命令使脚本仍运行在本终端，使得脚本中exit命令为退出本终端
            ]
        )
        self.ssh_client.invoke_shell(command)

    def run(self):
        self.build_app()
        self.connect_ssh()
        self.diff_and_copy()
        self.start_gdbserver()


def main():
    debugger = Debugger()
    debugger.run()


if __name__ == "__main__":
    main()
