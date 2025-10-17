import paramiko
import os
import select
import socket
from threading import Lock
from common_utils import throttle, md5_file, LineProgressBar, ProgressBarHelper
from concurrent.futures import ThreadPoolExecutor
from queue import Queue
from typing import Tuple


class SSHObject:
    def __init__(self, hostname, port, username, password):
        self.hostname = hostname
        self.port = port
        self.username = username
        self.password = password
        self.ssh = None
        self.sftp = None

    def __del__(self):
        self.close()

    def __enter__(self) -> Tuple[paramiko.SSHClient, paramiko.SFTPClient]:
        self.connect()
        return self.ssh, self.sftp

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def connect(self):
        self.ssh = paramiko.SSHClient()
        self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.ssh.connect(
            hostname=self.hostname,
            port=self.port,
            username=self.username,
            password=self.password,
        )
        self.ssh.get_transport().set_keepalive(15)  # 设置心跳，避免传输大文件时容易超时
        self.sftp = self.ssh.open_sftp()

    def close(self):
        if self.sftp:
            self.sftp.close()
            self.sftp = None
        if self.ssh:
            self.ssh.close()
            self.ssh = None


class SSHConnectionPool:
    def __init__(self, hostname, port, username, password, max_connections):
        self.hostname = hostname
        self.port = port
        self.username = username
        self.password = password
        self.max_connections = max_connections
        self.pool = Queue(max_connections)
        for _ in range(max_connections):
            self.pool.put(SSHObject(hostname, port, username, password))

    def get_connection(self) -> SSHObject:
        return self.pool.get()

    def release_connection(self, object: SSHObject):
        self.pool.put(object)


def sftp_put_resume(
    sftp: paramiko.SFTPClient,
    local_file: str,
    remote_file: str,
    progress_callback=None,
    overwrite: bool = False,
):
    """
    支持断点续传或强制覆盖的SFTP上传方法

    Args:
        sftp: paramiko.SFTPClient 实例
        local_file: 本地文件路径
        remote_file: 远程文件路径
        progress_callback: 回调函数，用于显示进度
        overwrite: 是否覆盖远程文件（True 表示覆盖）
    """
    total_size = os.path.getsize(local_file)
    remote_size = 0

    if not overwrite:
        # 非覆盖模式才检查断点
        try:
            remote_size = sftp.stat(remote_file).st_size
            if remote_size >= total_size:
                print(f"{local_file} 已完整上传.")
                return
            print(
                f"Resuming upload of {local_file} from {remote_size}/{total_size} bytes"
            )
        except FileNotFoundError:
            remote_size = 0
    else:
        # 覆盖模式：删除远程文件
        try:
            sftp.remove(remote_file)
        except FileNotFoundError:
            pass  # 文件不存在无需删除
        remote_size = 0

    block_size = 65536  # 64KB
    with open(local_file, "rb") as fl:
        fl.seek(remote_size)

        with sftp.file(remote_file, "ab" if not overwrite else "wb") as fr:
            while True:
                data = fl.read(block_size)
                if not data:
                    break
                fr.write(data)
                if progress_callback:
                    remote_size += len(data)
                    progress_callback(remote_size, total_size)


class SSHProcessor:
    def __init__(self, hostname, port, username, password):
        self.ssh_pool = SSHConnectionPool(
            hostname, port, username, password, max_connections=7
        )
        self.task_executor = ThreadPoolExecutor(
            max_workers=self.ssh_pool.max_connections
        )
        self.progress_helper = ProgressBarHelper()
        self.client = self.ssh_pool.get_connection()
        self.__lock = Lock()  # 保护client

    def connect(self):
        """连接到SSH服务器"""
        with self.__lock:
            self.client.connect()

    def close(self):
        """关闭SSH和SFTP连接"""
        with self.__lock:
            self.client.close()

    def get_remote_md5(self, remote_file, ssh: paramiko.SSHClient = None):
        """获取远程文件的MD5值"""

        # 调用远程环境md5sum计算
        def get_md5(ssh: paramiko.SSHClient):
            cmd = "command -v md5sum"
            _, stdout, _ = ssh.exec_command(command=cmd)
            if stdout.read().decode("utf-8"):
                cmd = f"md5sum {remote_file}"
                _, stdout, _ = ssh.exec_command(command=cmd)
                return stdout.read().decode("utf-8").split(" ")[0]
            return None

        if ssh:
            return get_md5(ssh)
        else:
            with self.__lock:
                return get_md5(self.client.ssh)

    def create_remote_dir(self, remote_dir, sftp: paramiko.SFTPClient = None):
        """创建远程目录"""

        def create_dir(sftp: paramiko.SFTPClient):
            try:
                sftp.stat(remote_dir)
            except FileNotFoundError:
                # 递归创建父目录
                self.create_remote_dir(os.path.dirname(remote_dir), sftp=sftp)
                sftp.mkdir(remote_dir)

        if sftp:
            create_dir(sftp)
        else:
            with self.__lock:
                create_dir(self.client.sftp)

    def transfer_file(self, local_file, remote_file):
        """传输单个文件并检查MD5值"""
        ssh_object = self.ssh_pool.get_connection()
        max_retries = 3  # 因超时重试的最大次数
        start_time = 0.0  # 缓存开始时间，用于断点续传时进度条正确统计
        while max_retries > 0:
            try:
                with ssh_object as (ssh, sftp):
                    # 设置超时时间，传输卡住不动后会超时
                    sftp.sock.settimeout(7)
                    # 确保远程目录存在
                    self.create_remote_dir(os.path.dirname(remote_file), sftp=sftp)
                    # # 计算本地文件的MD5值
                    local_md5 = md5_file(local_file)
                    # 获取远程文件的MD5值
                    remote_md5 = self.get_remote_md5(remote_file, ssh=ssh)
                    # 比较MD5值
                    if remote_md5 is None or local_md5 != remote_md5:
                        progress_bar = LineProgressBar(
                            total=os.path.getsize(local_file),
                            title=os.path.basename(local_file),
                        )
                        self.progress_helper.put(local_md5, progress_bar)

                        @throttle(
                            1, condition=lambda transferred, total: transferred >= total
                        )
                        def progress_callback(transferred, total):
                            self.progress_helper.update(local_md5, transferred)

                        if max_retries == 3:
                            start_time = progress_bar.start_time
                            sftp.put(
                                local_file, remote_file, callback=progress_callback
                            )
                        else:
                            progress_bar.start_time = start_time
                            # 使用断点续传上传文件
                            sftp_put_resume(
                                sftp,
                                local_file,
                                remote_file,
                                progress_callback=progress_callback,
                                overwrite=False,
                            )
                    break
            except socket.timeout:
                print(
                    f"Timeout occurred while transferring {local_file} to {remote_file}"
                )
                max_retries -= 1
                if max_retries > 0:
                    print("Retrying...")
                else:
                    print("Max retries exceeded. Aborting.")
                    break
            except Exception as e:
                print(f"Error transferring {local_file} to {remote_file}: {e}")
                break
        self.ssh_pool.release_connection(ssh_object)

    def transfer_directory(self, local_path, remote_path):
        """传输文件夹及其内容"""
        if not os.path.isdir(local_path):
            raise ValueError("local_path must be a directory")
        for root, dirs, files in os.walk(local_path):
            for file in files:
                local_file = os.path.join(root, file)
                relative_path = os.path.relpath(local_file, local_path)
                remote_file = os.path.join(remote_path, relative_path)
                self.task_executor.submit(self.transfer_file, local_file, remote_file)

    def transfer_file_or_directory(self, local_path, remote_path):
        """传输单个文件或文件夹"""
        if os.path.isdir(local_path):
            self.transfer_directory(local_path, remote_path)
        else:
            self.task_executor.submit(self.transfer_file, local_path, remote_path)

    def transfer_file_map(self, file_map):
        """根据文件映射表传输文件或文件夹"""
        for local_path, remote_path in file_map.items():
            self.transfer_file_or_directory(local_path, remote_path)

    def wait_transfer_done(self):
        """等待所有文件传输完成"""
        self.task_executor.shutdown(wait=True)

    def invoke_shell(self, command):
        """打开交互式shell"""
        with self.__lock:
            channel = self.client.ssh.invoke_shell(width=9999, height=9999)
            channel.send(command + "\n")
            while True:
                rlist, _, _ = select.select([channel], [], [])
                if channel in rlist:
                    if channel.recv_ready():
                        resp = channel.recv(1024)
                        # 使用utf-8解码，失败部分替换为'?'
                        content = resp.decode("utf-8", errors="replace")
                        if content:
                            print(content, end="")
                    if channel.exit_status_ready():
                        print(f"shell exit {channel.exit_status}")
                        break
            channel.close()

    def exec_command(self, command, is_echo=True, return_error=False):
        with self.__lock:
            host = f"{self.client.username}@{self.client.hostname}:{self.client.port}"
            if is_echo:
                print(f"{host} << {command}")
            _, stdout, stderr = self.client.ssh.exec_command(
                command=command, get_pty=True
            )
        res_out = stdout.read().decode("utf-8")
        res_err = stderr.read().decode("utf-8")
        if res_out:
            if is_echo:
                print(f"{host} >> {res_out}", end="", flush=True)
            return res_out
        elif res_err:
            print(f"{host} execute error <{res_err}>")
            if return_error:
                return None
            else:
                raise Exception()
