#!/usr/bin/env python
"""Regression contract for the profile-selector card layers."""
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "launcher" / "src" / "desktop_view.cpp"


def profile_selector_source() -> str:
    source = SOURCE.read_text(encoding="utf-8")
    start = source.index("void render_profile_select(")
    end = source.index("void render_home(", start)
    return source[start:end]


def main() -> int:
    selector = profile_selector_source()
    assert "const SDL_Rect name_panel" in selector, (
        "profile selector must retain the intended text label panel"
    )
    assert "const SDL_Rect shadow" not in selector, (
        "profile selector must not draw a separate gray avatar shadow bar"
    )
    print("Profile selector visual contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
