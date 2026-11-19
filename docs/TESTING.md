# Проверки R2

Текущий проход остановлен пользователем. Подробные результаты и обязательный
порядок возобновления: [контрольная точка](PAUSE_CHECKPOINT.md).

Переносимые C-проверки: `CJSON_DIR=/path/to/cJSON-1.7.19 bash tools/test_firmware_host.sh`.
Проверка JavaScript/SHA256: `node tools/check_web.cjs`.
Тест телефона: `npm ci --prefix web`, установка Chromium через Playwright,
затем `node tests/test_phone.cjs`; нужен build/tests/default-profile.json из C-теста.
Native simulator: команды сборки и CTest в [simulator/README.md](../simulator/README.md).

Отчёты в docs/reports и KiCad/reports, оставшиеся от предыдущего прохода,
не подтверждают последнюю изменённую силовую схему. Производственные экспорты
нужно обновить после финального ERC/DRC и сравнения netlist.
