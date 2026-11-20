# Hunter Reference preview — готовые файлы

Ветка `feat/hunter-reference-ui`, [PR №3](https://github.com/paradisemarketwb/bk-mikas-7.2/pull/3).
В main не объединено. Версия прошивки `2.2.1-uaz21-preview`.

- [Windows x64: bk_sim.exe + SDL2](BK-MIKAS-Hunter-Windows-x64-preview.zip) · [SHA256](BK-MIKAS-Hunter-Windows-x64-preview.zip.sha256).
- [ESP32-S3: safe / UART-live / USB-KKL](BK-MIKAS-Hunter-ESP32S3-preview.zip) · [SHA256](BK-MIKAS-Hunter-ESP32S3-preview.zip.sha256).
- [Галерея LVGL](../docs/images/hunter-reference/reference-contact.png), [полный отчёт](../docs/lvgl/HUNTER_STATUS.md).

Исходники бинарников: `33de17b7af80cc5deceb781ea1901669d43ecbe2`.
Проверенные CI: [Windows](https://github.com/paradisemarketwb/bk-mikas-7.2/actions/runs/44694755286)
и [ESP32/host](https://github.com/paradisemarketwb/bk-mikas-7.2/actions/runs/44694755292).
Бинарники скопированы из успешных Actions, не собраны заново при упаковке.
Ресурсы и размеры взяты из пользовательского UAZ21-комплекта; [сверка и отличия](../docs/lvgl/UAZ21_DESIGN.md).
Внутри ZIP: BUILD-METADATA.json и SHA256SUMS.txt на все включённые файлы.

Windows: распаковать весь архив в доступную для записи папку. Запустить
UAZ21.cmd для нового дизайна, либо Reference.cmd / Classic.cmd. Каждый запускатель возвращает свой сохранённый
JSON-профиль. Для программы без загрузчика — bk_sim.exe. Подробности в
WINDOWS-BUNDLE-RU.md внутри архива. Данные симуляционные; CI работал на Windows
Server 2025 с Visual Studio 2026, ручной запуск именно Windows 10 не выполнялся.

ESP32: выбрать **один** режим и читать READ-BEFORE-FLASH-RU.md. Для существующего
прибора сначала экспортировать настройки/backup; [инструкция миграции](../docs/hunter-background-migration.md)
сохраняет прежние адреса NVS/OTA. OTA приложения не обновляет таблицу разделов.
Физических испытаний питания, кабеля, сенсора и прошивки на плате не было.

Упаковка воспроизводится `tools/package_hunter_preview.py` из скачанных CI
артефактов. Скрипт проверяет PE x64 и адреса шести разделов в каждом режиме,
добавляет инструкции/лицензии и контрольные суммы.

---

## Предыдущий USB-KKL комплект R2

[USB-KKL / FT232RL experimental](BK-MIKAS-USB-KKL-FT232RL-experimental.zip)
и [SHA256](BK-MIKAS-USB-KKL-FT232RL-experimental.zip.sha256).

Бинарники из успешного CI run44595725508, исходники6716b7b, версия2.1.0-usb-kkl.
В ZIP включены flasher_args, команда esptool, инструкции/схема, руководство по
экранам и лицензии зависимостей. Это отдельный USB Host образ, не UART/L9637D.

Перед прошивкой полностью снять R2-жгут; прошивать дисплей через CH343/J2,
не KKL. Питание и линии USB описаны в [инструкции](../docs/firmware/USB_KKL.md).
Физического испытания кабеля с ESP32 не проводилось. Три firmware-конфигурации,
host и KiCad прошли проверки в [PR№2](https://github.com/paradisemarketwb/bk-mikas-7.2/pull/2).
