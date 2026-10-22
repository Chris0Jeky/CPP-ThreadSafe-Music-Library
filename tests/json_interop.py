"""Cross-check C++ import/export against Python's independent JSON implementation."""
import json
from pathlib import Path
import subprocess
import sys

tracks = [
    {"title": title, "artist": "Beyonc\u00e9", "duration": 180,
     "album": "Album } { \\\"\n" + title, "genre": title}
    for title in ["Caf\u00e9", "\u0218tiin\u021b\u0103", "\u041c\u0443\u0437\u044b\u043a\u0430",
                  "\u97f3\u697d", "\U0001f3b5", "A } B", "{live}", 'quote " slash \\']
]
source = Path("interop-input.json")
destination = Path("interop-output.json")
try:
    for ascii_only in (True, False):
        source.write_text(json.dumps(tracks, ensure_ascii=ascii_only), encoding="utf-8")
        subprocess.run([sys.argv[1], str(source), str(destination)], check=True)
        actual = json.loads(destination.read_text(encoding="utf-8"))
        assert actual == tracks, (actual, tracks)
finally:
    source.unlink(missing_ok=True)
    destination.unlink(missing_ok=True)
