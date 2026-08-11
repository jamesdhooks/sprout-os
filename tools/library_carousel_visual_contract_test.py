#!/usr/bin/env python3
"""Guard the 640x480 library's carousel-discovery contract."""

from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "launcher" / "src" / "desktop_view.cpp"


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8")
    start = source.index("void render_library(")
    end = source.index("void render_profile_archive(", start)
    library = source[start:end]

    forbidden_hints = (
        "START  MENU",
        "D-PAD BROWSE",
        "A PLAY",
        "B PROFILES",
    )
    for hint in forbidden_hints:
        assert hint not in library, f"library must not explain controls with {hint!r}"

    required_labels = (
        '"CONTINUE"',
        '"FAVORITES"',
        '"ALL GAMES"',
        '"SPROUT ARCADE"',
    )
    for label in required_labels:
        assert label in library, f"library section title {label} is missing"

    assert "previous_section" in library, (
        "library must expose the previous section title above the active rail"
    )
    assert "next_section" in library, (
        "library must expose the next section title at the bottom of the screen"
    )

    print("Library carousel visual contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
