import os


def getVsFiles(folder):
    out = []
    for entry in os.scandir(folder):
        if entry.is_file():
            # .sln and .vcxproj
            if ".sln" in entry.name or ".vcxproj" in entry.name:
                out.append(entry)
    return out


if __name__ == '__main__':
    root = os.path.realpath(os.path.dirname(__file__) + "/../")
    vs_files = getVsFiles(root)
    for entry in os.scandir(root):
        if entry.is_dir():
            vs_files += getVsFiles(entry.path)

    for f in vs_files:
        print("Deleting " + f.path)
        os.remove(f.path)
    input("Press enter to exit...")
