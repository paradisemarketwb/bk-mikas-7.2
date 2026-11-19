# Готовый USB-KKL комплект

[USB-KKL / FT232RL experimental](BK-MIKAS-USB-KKL-FT232RL-experimental.zip)
и [SHA256](BK-MIKAS-USB-KKL-FT232RL-experimental.zip.sha256).

Бинарники из успешного CI run44595725508, исходники6716b7b, версия2.1.0-usb-kkl.
В ZIP включены flasher_args, команда esptool, инструкции/схема, руководство по
экранам и лицензии зависимостей. Это отдельный USB Host образ, не UART/L9637D.

Перед прошивкой полностью снять R2-жгут; прошивать дисплей через CH343/J2,
не KKL. Питание и линии USB описаны в [инструкции](../docs/firmware/USB_KKL.md).
Физического испытания кабеля с ESP32 не проводилось. Три firmware-конфигурации,
host и KiCad прошли проверки в [PR№2](https://github.com/paradisemarketwb/bk-mikas-7.2/pull/2).
