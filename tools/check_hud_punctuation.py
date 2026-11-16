"""Check user-facing runtime colons and embedded C++ fallback consistency.

Run: python tools/check_hud_punctuation.py
"""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
locales = {
    language: json.loads((root / "localization" / "runtime" / f"{language}.json").read_text(encoding="utf-8"))
    for language in ("ko", "en")
}
assert locales["ko"].keys() == locales["en"].keys(), "Runtime locale key mismatch"
for language, entries in locales.items():
    for key, message in entries.items():
        remainder = message.replace("LDA:R", "")
        assert ":" not in remainder, f"{language}/{key}: extra colon in {message!r}"
        assert message.strip(), f"{language}/{key}: empty message"

# Reproduce generate_locales.py so the fallback map cannot silently go stale.
lines = ["#pragma once", "namespace DanceStudio::L10n {"]
for language, entries in locales.items():
    pairs = ",".join(
        "{" + json.dumps(key) + "," + json.dumps(value, ensure_ascii=False) + "}"
        for key, value in entries.items()
    )
    lines.append(f"inline const std::unordered_map<std::string,std::string> fallback_{language}={{{pairs}}};")
lines.append("}")
expected = "\n".join(lines) + "\n"
actual = (root / "runtime" / "LocalizationDefaults.h").read_text(encoding="utf-8")
assert actual == expected, "Runtime localization fallback is stale: run generate_locales.py"
print("PASS: runtime punctuation and offline localization fallback")
