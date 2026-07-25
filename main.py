import os
import sys
import subprocess

if __name__ == "__main__":
    args = sys.argv[1:]

    if not args:
        print("Usage: main.py <file.upp>", file=sys.stderr)
        raise SystemExit(1)

    target_file = args[-1]
    curr_dir = os.path.dirname(os.path.abspath(__file__))
    upp_exe = os.path.join(curr_dir, "upp.exe")

    if os.path.exists(upp_exe):
        res = subprocess.run([upp_exe, target_file])
        sys.exit(res.returncode)
    else:
        from upp.uplusplus.main import main as uplusplus_main
        if len(args) == 1:
            args = ["run", args[0]]
        raise SystemExit(uplusplus_main(args))