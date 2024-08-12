# -*- coding: utf-8 -*-
import os
import sys
import subprocess
import shutil
import logging
import fnmatch
from argparse import ArgumentParser

'''
生成步骤：
(1) 使用dump_syms对app生成app.sym，用法详见dump_syms -h
(2) 读取app.sym第1行内容，取其中的十六进制值生成文件夹symbols/<hex>，并将app.sym复制到该文件夹下
    内容示例：MODULE Linux arm64 FCFD3C221DDAA94ECF623B66566110D30 app，生成symbols/FCFD3C221DDAA94ECF623B66566110D30
    目录结构示例：
    └── symbols
        └── app
            └── FCFD3C221DDAA94ECF623B66566110D30
                └── app.sym
(3) 使用minidump_stackwalk对.dmp文件生成crash log
    minidump_stackwalk，用法详见minidump_stackwalk -h
(4) 若app有链接动态库，且有查看动态库堆栈的需求，则目录结构如下
└── symbols
        └── app
            └── FCFD3C221DDAA94ECF623B66566110D30
                └── app.sym
        └── libtest.so
            └── 64BA39BD7D4FE92942803334F8BCF3FB0
                libtest.so.sym
'''

WORKSPACE = os.path.dirname(__file__)
DUMP_SYMS_DEFAULT = os.path.join(WORKSPACE, "dump_syms")
MINIDUMP_STACKWALK_DEFAULT = os.path.join(WORKSPACE, "minidump_stackwalk")

def run_cmd(cmd, check=False):
    try:
        pid = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
        stdout, stderr = pid.communicate()
        if check and pid.returncode != 0:
            raise subprocess.CalledProcessError(pid.returncode, cmd)
        return stdout, stderr
    except Exception as e:
        print(f"Error executing command: {cmd}")
        raise

def find_shared_libraries(root_dir):
    return [os.path.join(dirpath, filename)
            for dirpath, _, filenames in os.walk(root_dir)
            for filename in fnmatch.filter(filenames, '*.so*')]

class Dumper:
    def __init__(self, app, dump_path, dump_syms, minidump_stackwalk, libs, libs_dir):
        self.appname = os.path.basename(app)
        self.app = os.path.abspath(app)
        self.dump_path = os.path.abspath(dump_path)
        self.symbols_path = os.path.join(WORKSPACE, "symbols")
        self.dump_syms = dump_syms
        self.minidump_stackwalk = minidump_stackwalk
        self.dump_list = []
        self.lib_list = self.init_libs(libs, libs_dir)

    def __del__(self):
        if os.path.exists(self.symbols_path):
            shutil.rmtree(self.symbols_path, ignore_errors=True)

    def init_libs(self, libs, libs_dir):
        lib_set = set(libs or [])
        if libs_dir:
            lib_set.update(find_shared_libraries(libs_dir))
        return list(lib_set)

    def get_dumps(self):
        self.dump_list = [os.path.join(self.dump_path, f) for f in os.listdir(self.dump_path) if f.endswith(".dmp")]

    def create_symbol(self, binary):
        # 生成sym文件
        bin = os.path.abspath(binary)
        name = os.path.basename(bin)
        sym = os.path.join(WORKSPACE, f"{name}.sym")
        run_cmd(f"{self.dump_syms} {bin} > {sym}")
        # 读取sym首行获取到module id值
        with open(sym, 'r') as f:
            module_id = next(f).split()[-2]
        # 创建目录symbols/<bin_name>/<module_id>，并将sym移动到该目录
        dir_path = os.path.join(self.symbols_path, name, module_id)
        os.makedirs(dir_path, exist_ok=True)
        shutil.move(sym, dir_path)
        sym_path_final = os.path.join(dir_path, f"{name}.sym")
        print(f"generate symbol {sym_path_final}")

    def create_stack(self, dump_file):
        stack_file = os.path.splitext(dump_file)[0] + ".stack"
        error_log = os.path.join(os.path.dirname(dump_file), "error.txt")
        run_cmd(f"{self.minidump_stackwalk} {dump_file} {self.symbols_path} > {stack_file} 2> {error_log}")
        print(f"generate stack for {dump_file} successfully: {stack_file}")

    def run(self):
        self.create_symbol(self.app)
        for lib in self.lib_list:
            self.create_symbol(lib)
        self.get_dumps()
        for dump in self.dump_list:
            self.create_stack(dump)

def main():
    parser = ArgumentParser(description="Process minidump files.")
    parser.add_argument("-a", "--app", dest="app", required=True, help="Path to the application binary")
    parser.add_argument("-d", "--dump-dir", dest="dump_path", required=True, help="Directory containing .dmp files")
    parser.add_argument("-l", "--libs", dest="libs", nargs='+', help="Path to the library binary")
    parser.add_argument("-L", "--libs-dir", dest="libs_dir", help="Path to the library binary")
    parser.add_argument("-s", "--dump-syms", dest="dump_syms", help="Path to dump_syms binary")
    parser.add_argument("-m", "--minidump-stackwalk", dest="minidump_stackwalk", help="Path to minidump_stackwalk binary")

    args = parser.parse_args()

    dump_syms = DUMP_SYMS_DEFAULT
    minidump_stackwalk = MINIDUMP_STACKWALK_DEFAULT
    if args.dump_syms:
        dump_syms = args.dump_syms
    if args.minidump_stackwalk:
        minidump_stackwalk = args.minidump_stackwalk

    try:
        dumper = Dumper(app=args.app,
                        dump_path=args.dump_path,
                        dump_syms=dump_syms,
                        minidump_stackwalk=minidump_stackwalk,
                        libs=args.libs,
                        libs_dir=args.libs_dir)
        dumper.run()
    except Exception as e:
        logging.exception(e)

if __name__ == "__main__":
    main()