import csv
from pathlib import Path
from urllib.parse import urlsplit

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
playlist_path = project_dir / "src" / "playlist.csv"
generated_dir = build_dir / "generated"
generated_dir.mkdir(parents=True, exist_ok=True)

stations = []
station_names = set()
station_urls = set()
with playlist_path.open(encoding="utf-8", newline="") as playlist_file:
    for line_number, row in enumerate(
        csv.reader(playlist_file, delimiter="\t"), start=1
    ):
        if not row or all(not field.strip() for field in row):
            continue
        if len(row) < 2:
            raise ValueError(
                f"{playlist_path}:{line_number}: expected station name and URL"
            )

        name, url = row[0].strip(), row[1].strip()
        if not name:
            raise ValueError(f"{playlist_path}:{line_number}: station name is empty")
        if len(name.encode("utf-8")) >= 32:
            raise ValueError(
                f"{playlist_path}:{line_number}: station name exceeds 31 UTF-8 bytes"
            )
        if len(url.encode("utf-8")) >= 192:
            raise ValueError(
                f"{playlist_path}:{line_number}: stream URL exceeds 191 UTF-8 bytes"
            )
        if urlsplit(url).scheme not in ("http", "https") or not urlsplit(url).netloc:
            raise ValueError(
                f"{playlist_path}:{line_number}: expected an absolute HTTP(S) stream URL"
            )
        if name.casefold() in station_names:
            raise ValueError(
                f"{playlist_path}:{line_number}: duplicate station name {name!r}"
            )
        if url in station_urls:
            raise ValueError(
                f"{playlist_path}:{line_number}: duplicate stream URL {url!r}"
            )
        station_names.add(name.casefold())
        station_urls.add(url)
        stations.append((name, url))

if not stations:
    raise ValueError(f"{playlist_path}: playlist contains no stations")
if len(stations) > 20:
    raise ValueError(
        f"{playlist_path}: {len(stations)} stations exceed the firmware limit of 20"
    )


def cpp_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


names = ",\n    ".join(cpp_string(name) for name, _ in stations)
urls = ",\n    ".join(cpp_string(url) for _, url in stations)
header = f"""#pragma once

constexpr unsigned int DEFAULT_STATION_COUNT = {len(stations)};
constexpr const char *DEFAULT_STATION_NAMES[] = {{
    {names},
}};
constexpr const char *DEFAULT_STATION_URLS[] = {{
    {urls},
}};
"""
(generated_dir / "default_playlist.h").write_text(header, encoding="utf-8")
env.Append(CPPPATH=[str(generated_dir)])
