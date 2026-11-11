# Facial Studio translation packs

Put a UTF-8 `.json` file in `Tools/FacialStudio/lang/`, restart Facial Studio, then select the new entry in the language dropdown. Discovery never selects a new language automatically. The selected ID is saved for the next launch.

```json
{"schemaVersion":1,"id":"fr","name":"Français","strings":{"저장":"Enregistrer","불러오기":"Ouvrir"}}
```

Use `translation-template.json.example` as your starting point: keep the Korean source keys, translate the English values, and set your own lowercase ID and native display name. Rename the copy to `.json`. The shipped template is not an installed or completed translation. Preserve `{0}`, `{1}`, etc. Names, IDs, paths, and user-authored preset/clip data are not translated.

Missing entries fall back to English, then the Korean source. Invalid files are ignored. `ko` and `en` are reserved; colliding external IDs are all omitted. A pack is at most 1 MiB; at most 64 JSON files are allowed in the folder. An overfull folder loads no external packs. IDs must match `[a-z][a-z0-9_-]{1,31}`. If the selected pack disappears, startup falls back to Korean.

Dance Studio and LDA:R use the same outer JSON shape but different translation keys; use the template for each product. Install a Windows font supporting your language if its characters are unavailable.
