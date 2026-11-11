# Translator language packs

Place UTF-8 `*.json` translation files in `Tools/DanceStudio/lang`. Restart Dance Studio, then choose the language from the Language / 언어 list. Adding a file never automatically switches your selected language. Selection is saved. The folder is scanned at startup; this is not a live file watcher.

Copy `translation-template.json.example` to e.g. `fr.json`, set `id` to `fr`, set `name` to `Français`, and translate only the values in `strings`. The template is intentionally not loaded until renamed. It contains the full English key list for this product. Each product has a different set of translation keys.

Format:

```json
{
  "schemaVersion": 1,
  "id": "fr",
  "name": "Français",
  "strings": { "REPLACE_WITH_A_KEY_FROM_THE_TEMPLATE": "Your translated text" }
}
```

- Language IDs: 2–32 lowercase ASCII letters/digits, `-` or `_`, starting with a letter. Built-in `ko` and `en` are reserved; use e.g. `en-custom` for a separate custom variant.
- Keep keys, file names, FormIDs and product names unchanged. Preserve numbered placeholders such as `{0}`, `{1}` and any `##...` or `###...` suffix exactly.
- A partial pack is allowed. Missing/invalid entries fall back to English, then Korean. Bad files are skipped; duplicate external IDs are all excluded. A missing previously selected pack falls back to Korean.
- Maximum 64 JSON files and 1 MiB per file. Duplicate JSON keys are invalid. Do not include empty/NUL strings.
- Source assets (HKX, NIF, TRI, DDS, music) do not belong in translation packs.
- Windows font fallback is used. Test longer labels in the actual editor window.

For LDA:R, the game Language choice is in SKSE Menu > LDA:R settings. For Dance Studio, use the top toolbar list. Facial Studio uses the upper-left list. Existing preferences and project content are not distributed in these packages.
