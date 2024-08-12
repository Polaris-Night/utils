# -*- coding: utf-8 -*-
import os
import sys
import subprocess
import shutil
import traceback
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

def handle_exception():
    (type, value, trace) = sys.exc_info()
    print("**************************************************************")
    print("Error_Type:\t", type)
    print("Error_Value:\t", value)
    print("%-40s %-20s %-20s %-20s\n" % ("Filename", "Function", "Linenum", "Source"))
    for filename, linenum, funcname, source in traceback.extract_tb(trace):
        print("%-40s %-20s %-20s%-20s" % (os.path.basename(filename), funcname, linenum, source))
    print("**************************************************************")

class Dumper:
    def __init__(self, app, dump_path, dump_syms, minidump_stackwalk, libs):
        self.appname = os.path.basename(app)
        self.app = os.path.abspath(app)
        self.dump_path = os.path.abspath(dump_path)
        self.symbols_path = os.path.join(self.dump_path, "symbols")
        self.dump_syms = dump_syms
        self.minidump_stackwalk = minidump_stackwalk
        self.dump_list = []
        self.lib_list = []
        if libs:
            self.lib_list = libs

    def __del__(self):
        if os.path.exists(self.symbols_path):
            shutil.rmtree(self.symbols_path, ignore_errors=True)

    def get_dumps(self):
        self.dump_list = [os.path.join(self.dump_path, f) for f in os.listdir(self.dump_path) if f.endswith(".dmp")]

    def create_symbol(self, binary):
        # 生成sym文件
        bin = os.path.abspath(binary)
        sym = f"{bin}.sym"
        cmd = f"{self.dump_syms} {bin} > {sym}"
        self.run_cmd(cmd)
        # 读取sym首行获取到module id值
        with open(sym, 'r') as f:
            line = next(f)
        module_id = line.split()[-2]
        # 创建目录symbols/<bin_name>/<module_id>，并将sym移动到该目录
        dir = os.path.join(self.symbols_path, os.path.basename(bin), module_id)
        os.makedirs(dir, exist_ok=True)
        shutil.move(sym, dir)
        sym_path_final = os.path.join(dir, f"{os.path.basename(bin)}.sym")
        print(f"generate symbol {sym_path_final}")

    def create_stack(self, dump_file):
        stack_file = os.path.splitext(dump_file)[0] + ".stack"
        error_log = os.path.join(os.path.dirname(dump_file), "error.txt")
        cmd = f"{self.minidump_stackwalk} {dump_file} {self.symbols_path} > {stack_file} 2> {error_log}"
        self.run_cmd(cmd)
        print(f"generate stack for {dump_file} successfully: {stack_file}")

    def run_cmd(self, cmd, check=False):
        try:
            pid = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
            stdout, stderr = pid.communicate()
            if check and pid.returncode != 0:
                raise subprocess.CalledProcessError(pid.returncode, cmd)
            return stdout, stderr
        except Exception as e:
            print(f"Error executing command: {cmd}")
            raise

    def run(self):
        self.create_symbol(self.app)
        for lib in self.lib_list:
            self.create_symbol(lib)
        self.get_dumps()
        for dump in self.dump_list:
            self.create_stack(dump)

def main():
    parser = ArgumentParser(description="Process minidump files.")
    parser.add_argument("-a", "--app", dest="app", help="Path to the application binary")
    parser.add_argument("-l", "--libs", dest="libs", nargs='+', help="Path to the library binary")
    parser.add_argument("-L", "--libs-dir", dest="libs_dir", help="Path to the library binary")
    parser.add_argument("-d", "--dump-dir", dest="dump_path", help="Directory containing .dmp files")
    parser.add_argument("-s", "--dump-syms", dest="dump_syms", help="Path to dump_syms binary")
    parser.add_argument("-m", "--minidump-stackwalk", dest="minidump_stackwalk", help="Path to minidump_stackwalk binary")

    args = parser.parse_args()

    if not args.app or not args.dump_path:
        parser.error("Both --app and --dump-dir are required")

    dump_syms = DUMP_SYMS_DEFAULT
    minidump_stackwalk = MINIDUMP_STACKWALK_DEFAULT
    if args.dump_syms:
        dump_syms = args.dump_syms
    if args.minidump_stackwalk:
        minidump_stackwalk = args.minidump_stackwalk

    app = args.app
    dump_path = args.dump_path

    try:
        Dumper(app=app,
               dump_path=dump_path,
               dump_syms=dump_syms,
               minidump_stackwalk=minidump_stackwalk,
               libs=args.libs).run()
    except Exception as e:
        handle_exception()

if __name__ == "__main__":
    main()