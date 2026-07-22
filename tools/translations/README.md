# Translation Scripts

- `qt-source.sh`: Regenerate the English source file for Qt.
  * After doing this, ensure to `tx push -s`.
  * You must have `lupdate` in your PATH, e.g. `export PATH="/usr/lib64/qt6/bin:$PATH"` (requires `qttools`)
- `update-translations.sh`: Update and commit translation files.