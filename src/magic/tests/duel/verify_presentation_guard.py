"""Check that renderer error dialogs fail immediately without showing a window."""
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--binary", type=Path, required=True)
parser.add_argument("--node", type=Path)
args = parser.parse_args()
binary = args.binary.resolve()
command = ([str(args.node)] if args.node else []) + [str(binary), "--duel-presentation-probe"]
result = subprocess.run(command, cwd=binary.parent, capture_output=True, text=True, timeout=10)
assert result.returncode != 0, "The headless presentation guard did not stop the renderer"
assert "DUEL_HARNESS_ERROR: headless presentation probe" in result.stderr, result.stdout + result.stderr
print("Renderer popup was stopped at the presentation boundary.")
