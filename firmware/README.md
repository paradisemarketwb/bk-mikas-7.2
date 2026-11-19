# Прошивка R2

ESP-IDF 5.5.1, ESP32-S3, LVGL 9.6.0. [Сборка и поведение](../docs/firmware/R2.md),
[результаты проверок](../docs/R2_STATUS.md), [LVGL Simulator](../simulator/README.md).

`main/` объединяет независимые worker K-Line, снимки телеметрии и UI.
`components/mikas/` — monitor0B codec; `kline/` — UART с обязательным echo;
`telemetry/` — статусы/возраст/формулы; `settings/` — NVS;
`wifi/` — WPA2 AP и read-only HTTP; `board/` — Waveshare LCD/CST820;
`ui_core/`, `ui_widgets/`, `ui_screens/` — общий LVGL код компьютера и ESP32.

K-Line/Wi-Fi по умолчанию отключены в menuconfig. Аппаратная безопасность GPIO19,
питания и UART должна быть проверена до включения физического обмена.
OTA и пользовательский редактор конфигураций в прошивке пока отсутствуют.
`ui_layout/` и старое браузерное демо — прежний прототип, не LVGL simulator.
