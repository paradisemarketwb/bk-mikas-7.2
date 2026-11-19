# Действующая прошивка R2

ESP-IDF5.5.1 / LVGL9.6.0 / ESP32-S3. [Инструкция Windows10](../docs/firmware/R2.md),
[телефон и OTA](../docs/getting-started/PHONE.md), [статус](../docs/R2_STATUS.md).

- `mikas` и `kline`: собственный monitor0B,9600,8N1, framing/echo, только чтение.
- `telemetry` и `history`: проверенные формулы, источник/возраст, ограниченные журналы.
- `ui_config`, `ui_widgets`, `ui_core`, `ui_screens`: переносимые декларативные экраны.
- `board`: аппаратные драйверы Waveshare ST7701/TCA9554/CST820, RGB/PSRAM и PWM.
- `settings`: общий JSON/validator/defaults и ESP-хранилище NVS.
- `wifi`: WPA2 AP, HTTP авторизация, веб-конструктор, импорт/экспорт и OTA.
- `main`: задачи, очереди, snapshots, безопасное отключение K-Line при обновлении.

Wi-Fi включён по умолчанию, K-Line — только после аппаратного стенда.
Старые схемы/layout-generator/browserdemo находятся в archive/r1b.
Сборка и симулятор не доказывают физическую работу в автомобиле.
