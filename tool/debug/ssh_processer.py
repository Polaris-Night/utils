import paramiko
import os
import time
import select
from common_utils import md5_file, ProgressBar


class SSHProcessor:
    def __init__(self, hostname, port, username, password):
        self.hostname = hostname
        self.port = port
        self.username = username
        self.password = password
        self.ssh = None
        self.sftp = None

    def __del__(self):
        self.close()

    def connect(self):
        """连接到SSH服务器"""
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.ssh.connect(
            hostname=self.hostname,
            port=self.port,
            username=self.username,
            password=self.password,
        )
        self.sftp = self.ssh.open_sftp()

    def close(self):
        """关闭SFTP和SSH连接"""
        if self.sftp:
            self.sftp.close()
        if self.ssh:
            self.ssh.close()

    def get_remote_md5(self, remote_file):
        """获取远程文件的MD5值"""

        # 远程环境md5sum可用则调用md5sum计算
        cmd = "command -v md5sum"
        _, stdout, _ = self.ssh.exec_command(cmd)
        if stdout.read().decode("utf-8"):
            cmd = f"md5sum {remote_file}"
            _, stdout, _ = self.ssh.exec_command(cmd)
            return stdout.read().decode("utf-8").split(" ")[0]
        return None

    def create_remote_dir(self, remote_dir):
        """创建远程目录"""
        try:
            self.sftp.stat(remote_dir)
        except FileNotFoundError:
            self.create_remote_dir(os.path.dirname(remote_dir))
            self.sftp.mkdir(remote_dir)

    def transfer_file(self, local_file, remote_file):
        """传输单个文件并检查MD5值"""
        # 确保远程目录存在
        remote_dir = os.path.dirname(remote_file)
        self.create_remote_dir(remote_dir)

        # 计算本地文件的MD5值
        local_md5 = md5_file(local_file)

        # 获取远程文件的MD5值
        remote_md5 = self.get_remote_md5(remote_file)

        # 比较MD5值
        if remote_md5 is None or local_md5 != remote_md5:
            progress_bar = ProgressBar(
                os.path.getsize(local_file), os.path.basename(local_file)
            )
            last_update_time = time.time()

            def progress_callback(transferred, total):
                nonlocal last_update_time
                current_time = time.time()
                if current_time - last_update_time >= 1 or transferred >= total:
                    progress_bar.update(transferred)
                    last_update_time = current_time

            self.sftp.put(local_file, remote_file, progress_callback)

    def transfer_directory(self, local_path, remote_path):
        """传输文件夹及其内容"""
        assert os.path.isdir(local_path), "local_path must be a directory"
        for root, dirs, files in os.walk(local_path):
            for file in files:
                local_file = os.path.join(root, file)
                relative_path = os.path.relpath(local_file, local_path)
                remote_file = os.path.join(remote_path, relative_path)
                self.transfer_file(local_file, remote_file)

    def transfer_file_or_directory(self, local_path, remote_path):
        """传输单个文件或文件夹"""
        if os.path.isdir(local_path):
            self.transfer_directory(local_path, remote_path)
        else:
            self.transfer_file(local_path, remote_path)

    def transfer_file_map(self, file_map):
        """根据文件映射表传输文件或文件夹"""
        for local_path, remote_path in file_map.items():
            self.transfer_file_or_directory(local_path, remote_path)

    def invoke_shell(self, command):
        """打开交互式shell"""
        channel = self.ssh.invoke_shell(width=9999, height=9999)
        channel.send(command + "\n")
        while True:
            rlist, _, _ = select.select([channel], [], [])
            if channel in rlist:
                if channel.recv_ready():
                    resp = channel.recv(1024)
                    content = resp.decode("utf-8")
                    if content:
                        print(content, end="")
                if channel.exit_status_ready():
                    print(f"shell exit {channel.exit_status}")
                    break
        channel.close()

    def exec_command(self, command, is_echo=True, return_error=False):
        host = "{username}@{hostname}:{port}".format(
            port=self.port, username=self.username, hostname=self.hostname
        )
        if is_echo:
            print("{host} << {command}".format(command=command, host=host))
        _, stdout, stderr = self.ssh.exec_command(command=command, get_pty=True)
        res_out = stdout.read().decode("utf-8")
        res_err = stderr.read().decode("utf-8")
        if res_out:
            if is_echo:
                print(host, ">>", res_out, end="", flush=True)
            return res_out
        elif res_err:
            print("{host} 执行错误 <{err}>".format(host=host, err=res_err))
            if return_error:
                return None
            else:
                raise Exception()
