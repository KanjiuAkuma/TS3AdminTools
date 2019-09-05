import sys
import os

if __name__ == '__main__':
    root = os.path.realpath(os.path.dirname(__file__) + "/../")
    os.chdir(root)
    premake_path = root + "\\util\\premake\\premake5.exe"
    premake_args = "vs2017"
    os.system("%s %s" % (premake_path, premake_args))
    input("Press enter to exit...")
