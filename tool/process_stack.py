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
        └── FCFD3C221DDAA94ECF623B66566110D30
            └── app.sym
(3) 使用minidump_stackwalk对.dmp文件生成crash log
    minidump_stackwalk，用法详见minidump_stackwalk -h
'''

DUMP_SYMS_DEFAULT = "./dump_syms"
MINIDUMP_STACKWALK_DEFAULT = "./minidump_stackwalk"

def handle_exception():
    error_message = ""
    (type, value, trace) = sys.exc_info()
    print("**************************************************************")
    print("Error_Type:\t", type)
    print("Error_Value:\t", value)
    print("%-40s %-20s %-20s %-20s\n" % ("Filename", "Function", "Linenum", "Source"))
    for filename, linenum, funcname, source in traceback.extract_tb(trace):
        print("%-40s %-20s %-20s%-20s" % (os.path.basename(filename), funcname, linenum, source))
    print("**************************************************************")

class DumpProcesser:
    def __init__(self, app, dump_path, dump_syms, minidump_stackwalk):
        self.current = os.path.abspath(os.path.dirname(app))
        self.appname = os.path.basename(app)
        self.app = os.path.abspath(app)
        self.dump_path = os.path.abspath(dump_path)
        self.dump_syms = dump_syms
        self.minidump_stackwalk = minidump_stackwalk
        self.symbols_path = os.path.join(self.dump_path, "symbols")
        self.sym_file = f"{self.app}.sym"
        self.dump_file_list = []

    def __del__(self):
        if os.path.exists(self.symbols_path):
            shutil.rmtree(self.symbols_path, ignore_errors=True)
        if os.path.exists(self.sym_file):
            os.remove(self.sym_file)

    def get_dumps(self):
        self.dump_file_list = [os.path.join(self.dump_path, f) for f in os.listdir(self.dump_path) if f.endswith(".dmp")]

    def create_sym(self):
        cmd = f"{self.dump_syms} {self.app} > {self.sym_file}"
        self.run_cmd(cmd, check=True)
        print(f"generate sym file: {self.sym_file}")

    def read_line(self):
        symfile = self.sym_file
        with open(symfile, 'r') as fd:
            line = next(fd)
        linemd5 = line.split()[-2]
        mkdirpath = os.path.join(self.symbols_path, self.appname, linemd5)
        os.makedirs(mkdirpath, exist_ok=True)
        shutil.copy(symfile, mkdirpath)
        print(f"generate symbols dir: {mkdirpath}")

    def create_dump(self, dump_file):
        symbols_path = self.symbols_path
        crash_log = os.path.join(os.path.splitext(dump_file)[0] + ".log")
        error_log = os.path.join(self.current, "error.log")
        cmd = f"{self.minidump_stackwalk} {dump_file} {symbols_path} > {crash_log} 2> {error_log}"
        self.run_cmd(cmd)
        print(f"generate crash log for {dump_file} successfully: {crash_log}")

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
        self.create_sym()
        self.read_line()
        self.get_dumps()
        for dump in self.dump_file_list:
            self.create_dump(dump)

def main():
    parser = ArgumentParser(description="Process minidump files.")
    parser.add_argument("-a", "--app", dest="app", help="Path to the application binary")
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
        DumpProcesser(app=app, dump_path=dump_path,
                      dump_syms=dump_syms, minidump_stackwalk=minidump_stackwalk).run()
    except Exception as e:
        handle_exception()

if __name__ == "__main__":
    main()