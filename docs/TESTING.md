# Проверки R2

Проверки исходников, симуляторов и правил PCB не заменяют испытания готового
устройства. Результаты привязаны к ревизии исходников и опубликованным GitHub Actions.

## Прошивка, JSON и журналы

Нужны C11-компилятор и cJSON 1.7.19, commit
`c859b25da02955fef659d658b8f324b5cde87be3`:

```bash
CJSON_DIR=/path/to/cJSON bash tools/test_firmware_host.sh
```

ASan/UBSan проверяют codec monitor0B, escaping/checksum, запрет записи,
декодеры, актуальность данных, JSON roundtrip, дубли ключей, недопустимую
геометрию, кольцевую историю и события ошибок. Тест также создаёт реальный
заводской JSON-профиль для браузерного сценария.

## LVGL Simulator

```bash
cmake -S simulator -B build/simulator -DBK_WITH_SDL=OFF
cmake --build build/simulator -j2
ctest --test-dir build/simulator --output-on-failure
```

Проверяются рендеры десяти видов в двух темах, настоящие pointer-события меню
и редактора, применение конфигурации, все девять типов виджетов, ограниченная
история, неблокирующая заставка и import/export JSON. Интерактивная сборка
и инструкция Windows — в [simulator/README.md](../simulator/README.md).

## Телефон

```bash
node tools/check_web.cjs
npm ci --prefix web
npm exec --prefix web -- playwright install --with-deps chromium
node tests/test_phone.cjs
```

Сценарий использует локальный mock API: вход, показания, изменение и сохранение
профиля, экспорт JSON и подтверждённая загрузка OTA с SHA256. Для системного
Chromium можно задать BK_CHROMIUM. Снимки помечены как симуляция.
Эта проверка не подтверждает работу радио или запись OTA на физический ESP32.

## ESP-IDF

GitHub Actions собирает safe и live с ESP-IDF5.5.1/LVGL9.6.0 из manifest.
Safe: K-Line выключен; live: включён. Wi-Fi включён в обоих вариантах.
Компоненты получаются через реестр, поэтому дополнительно проверяется layout
управляемых зависимостей. Инструкция локальной сборки — [firmware/R2.md](firmware/R2.md).

## KiCad, механика и жгут

Для обеих плат выполняются ERC, DRC, экспорт XML netlist и сравнение со всеми
подключёнными площадками PCB. Gerber и Excellon должны быть экспортированы
из проверенного PCB той же ревизии. Проверка жгута:

```bash
/usr/bin/python3 tools/check_r2_hardware.py
python3 tools/check_case_mesh.py
python3 tools/render_system_diagrams.py --check
```

Для netlist-проверки нужен Python KiCad с pcbnew. Mesh-проверка ищет несколько
несвязанных оболочек и незамкнутые рёбра в STL; она не проверяет реальную
посадку. XML и отчёты находятся в reports соответствующего KiCad-проекта.

## Испытания с оборудованием

Нужно измерить OV/UV/переполюсовку/импульсы и нагрев питания, обратный ток USB,
поведение UART/ARM при ROM/BOOT/reset/brownout, реальные токи дисплея,
RGB/сенсор/PSRAM при Wi-Fi, посадку корпуса и данные конкретного ЭБУ.
Эти испытания в рамках программной среды не проводились. Список и текущие
границы готовности — в [статусе R2](R2_STATUS.md).
