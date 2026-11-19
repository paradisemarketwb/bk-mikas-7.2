# Настоящий LVGL Simulator

Этот настольный C-процесс собирает **те же** `ui_core`, `ui_widgets`, `ui_screens`,
что прошивка. Версия LVGL: **9.6.0**, commit
`80ca777e37a2b176770726a02e07a6fb79ef0b39`.
Браузерный `ui/modular-demo.html` остаётся историческим прототипом.

## Windows 10

1. Установить Git, CMake 3.20+, Visual Studio 2022 Community с компонентом
   «Разработка классических приложений на C++» (Desktop development with C++).
2. Открыть Developer PowerShell. Клонировать репозиторий и перейти в его корень.
3. Для интерактивного окна установить SDL2 через vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg C:/dev/vcpkg
C:/dev/vcpkg/bootstrap-vcpkg.bat
C:/dev/vcpkg/vcpkg install sdl2:x64-windows
cmake -S simulator -B build/simulator -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/simulator --config Release
./build/simulator/Release/bk_sim.exe
```

Первое конфигурирование скачивает закреплённую LVGL из GitHub. Без SDL2 доступен
настоящий LVGL headless renderer, который сохраняет все экраны в PPM:

```powershell
cmake -S simulator -B build/simulator -A x64 -DBK_WITH_SDL=OFF
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

- Мышь — сенсор, горизонтальный свайп между пятью приборами.
- Нажатие на крупное показание — следующий параметр из реестра.
- Стрелки — все десять экранов; T — день/ночь; C — связь/устаревшие данные.
- +/− — обороты; W/S — температура; V — напряжение; D — три ошибки ЭБУ.
- Пробел — остановить периодическое обновление; Escape — выход.

Показания в симуляторе искусственные. Скорость, запас хода, пробег и средняя
скорость не генерируются. Пример QR использует явно демонстрационные реквизиты
`HUNTER-SIMULATION / SIMULATOR-ONLY`; прошивка передаёт собственные реквизиты AP
через API, пароля по умолчанию из симулятора в ней нет.

Сборка и headless запуск проверены на Linux (GCC 14.2, SDL2 runtime 2.32.4).
Windows-инструкции подготовлены; физический запуск на Windows не проверен.
