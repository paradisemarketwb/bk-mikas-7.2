# Настоящий LVGL Simulator

Этот настольный C-процесс собирает **те же** `ui_core`, `ui_widgets`, `ui_screens`,
что прошивка. Версия LVGL: **9.6.0**, commit
`80ca777e37a2b176770726a02e07a6fb79ef0b39`.
Браузерный `ui/modular-demo.html` остаётся историческим прототипом.

## Windows 10

1. Установить Git, CMake 4.2+, Visual Studio 2026 Community с компонентом
   «Разработка классических приложений на C++» (Desktop development with C++).
2. Открыть Developer PowerShell. Клонировать репозиторий и перейти в его корень.
3. Для интерактивного окна установить SDL2 через vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg C:/dev/vcpkg
C:/dev/vcpkg/bootstrap-vcpkg.bat
C:/dev/vcpkg/vcpkg install sdl2:x64-windows
cmake -S simulator -B build/simulator -G "Visual Studio 18 2026" -A x64 -DENABLE_CUSTOM_COMPILER_FLAGS=OFF -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DBK_REQUIRE_SDL=ON
cmake --build build/simulator --config Release
./build/simulator/Release/bk_sim.exe
```

Первое конфигурирование скачивает закреплённую LVGL из GitHub. Без SDL2 доступен
настоящий LVGL headless renderer, который сохраняет все экраны в PPM:

```powershell
cmake -S simulator -B build/simulator -G "Visual Studio 18 2026" -A x64 -DENABLE_CUSTOM_COMPILER_FLAGS=OFF -DBK_WITH_SDL=OFF
cmake --build build/simulator --config Release
./build/simulator/Release/bk_sim.exe --headless build/simulator
ctest --test-dir build/simulator -C Release --output-on-failure
```

PPM можно открыть в GIMP/IrfanView; готовые PNG находятся в
[`docs/images/lvgl`](../docs/images/lvgl/all-screens.png).

## Linux

Нужны GCC/G++, CMake, Ninja (необязательно), Git и `libsdl2-dev` для окна:

```sh
cmake -S simulator -B build/simulator
cmake --build build/simulator -j4
./build/simulator/bk_sim
ctest --test-dir build/simulator --output-on-failure
```

Для использования уже скачанной библиотеки добавить
`-DFETCHCONTENT_SOURCE_DIR_LVGL=/path/to/lvgl-9.6.0`.

## Управление

- Мышь — сенсор: вертикальный свайп меняет прибор; горизонтальный — основной параметр текущего прибора.
- Нажатие/долгое нажатие на виджет — редактор (тип, метрика, положение, диапазон, пороги, цвет).
- Стрелки — десять системных/исходных экранов; T — день/ночь; C — связь/устаревшие данные.
- +/− — обороты; W/S — температура; V — напряжение; D — три ошибки ЭБУ.
- Пробел — остановить периодическое обновление; Escape — выход.
- Настройки → Редактор экранов: добавить/удалить виджет, менять параметры и применить.
- Яркость в симуляторе сохраняется в UI-модели; PWM физической платы отсутствует.

Показания в симуляторе искусственные. Скорость, запас хода, пробег и средняя
скорость не генерируются. Пример QR использует явно демонстрационные реквизиты
`HUNTER-SIMULATION / SIMULATOR-ONLY`; прошивка передаёт собственные реквизиты AP
через API, пароля по умолчанию из симулятора в ней нет.

Сборка и headless запуск проверены на Linux (GCC 14.2, SDL2 runtime 2.32.4).
Сборка Windows x64 с VS2026 и headless запуск `.exe` проверяются в
[Hunter Windows CI](https://github.com/paradisemarketwb/bk-mikas-7.2/actions/workflows/hunter-windows.yml).
Артефакт `Hunter-Windows-x64-VS2026-preview` содержит `.exe` и `SDL2.dll`.
CI использует Windows Server 2025: ручной запуск интерактивного окна именно
на Windows 10 остаётся проверкой на компьютере пользователя.

## Проверка конструктора и экспорт изображений

`ctest` кроме20screenframes выполняет реальную последовательность касаний
редактора: выбор виджета → изменение типа ARC на NEEDLE → сохранение → сравнение
модели. Далее проверяются все9типов,64-point bounded history, отказ размещения
за границей круглого дисплея. Приветственный экран и его задержка удалены. Создаются также
`widget-00..08.ppm`, `layout-editor.ppm`, `widget-editor.ppm`.

Для PNGгалереи установить Pillow и выполнить из корня репозитория:

```sh
python -m pip install Pillow
python simulator/export_gallery.py build/simulator
```

Модель конструктора — `firmware/components/ui_config/include/bk_ui_config.h`;
рендерер не зависит от прошивочного транспорта. Полное описание полей и API:
[docs/lvgl/IMPLEMENTATION.md](../docs/lvgl/IMPLEMENTATION.md).

## Те же JSON-профили, что на приборе

Симулятор использует общие `settings/bk_profile.c` и `bk_profile_json.c`.
Закреплена cJSON1.7.19 (`c859b25da02955fef659d658b8f324b5cde87be3`).
Формат такой же, как экспорт телефонного конструктора.

```sh
./build/simulator/bk_sim --import simulator/profiles/default.json --export edited.json
./build/simulator/bk_sim --import simulator/profiles/engine-history.json
```

`--import` проверяет профиль до применения; `--export` сохраняет окончательный
результат редактирования при выходе из интерактивного окна. Профиль не содержит
паролей AP или показаний ЭБУ.

Пример `engine-history.json` добавляет шестой прибор со стрелкой и заменяет
экран двигателя на температуру + мини-график. Его можно импортировать и в прибор.
Предпросмотр без SDL/окна:

```sh
./build/simulator/bk_sim --import simulator/profiles/engine-history.json --render build/simulator --export roundtrip.json
```

`--render DIR` создаёт `custom-00..12.ppm`, включая дополнительные viewID10..12.
Каталог должен существовать. Параметры тем/яркости/всех виджетов сохраняются.
`--headless DIR` запускает регрессионные проверки на стандартной конфигурации;
для предпросмотра своего профиля используйте `--render`.

В CTest дополнительно проверяются JSON export и import теми же исходниками,
что в прошивке. Проверено равенство JSON до/после roundtrip.

## Hunter Reference preview

Ветка `feat/hunter-reference-ui`; до приёмки пользователя не сливается в main.
В одной программе доступны Classic R2 и Hunter Reference, стиль хранится у
каждого прибора. Старые JSON `version:3/layout:1` мигрируют в Classic;
новый экспорт `version:4/layout:2` содержит последовательности параметров,
калибровки и политики источников. Сам импорт не записывает профиль в прибор.

Для проверки сохранения обязательно задайте `--export my-profile.json` при
первом запуске, затем `--import my-profile.json --export my-profile.json`.
При завершении через Escape/крестик сохраняется окончательная компоновка.
Для переноса между компьютерами сохраните JSON и пользовательские ресурсы фонов.

[Калибровки и источники](../docs/sources-calibration.md),
[хранилище пользовательских фонов и миграция](../docs/hunter-background-migration.md).
