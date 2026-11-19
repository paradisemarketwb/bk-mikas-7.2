> **Историческая документация R1/R1B.** Не применять для сборки R2. Актуальные исходники и ограничения: [статус R2](R2_STATUS.md), [прошивка R2](firmware/R2.md), [аппаратная часть R2](../hardware/kicad/r2/README.md). Указания GPIO0 и старая распиновка не действуют для R2.

# Комплектующие собственного БК — ревизия R1B

**Заказывать компоненты по этому документу, а не по старым черновикам R0.** Готовый, проверенный именно для нашего БК K-Line↔UART модуль на L9637D на OZON/WB/AliExpress пока не подтверждён; поэтому проектируем свою плату. USB KKL / FT232 — **не равно** UART-модулю для ESP32-S3.

## Список деталей

| Код | Количество | Наименование, назначение | Дополнительно |
|---|---:|---|---|
| LCD | 1 | **Waveshare ESP32-S3-Touch-LCD-2.1** или 2.1B, круглый 480×480, сенсор | Предварительно проверять ревизию и 12PIN кабель |
| U1 | 1 | **ST E-L9637D / L9637D**, SO-8 | Физический интерфейс K-Line |
| U2 | 1 | **SN74LVC1G125-Q1**, SOT-23-5, 3,3 В | Аппаратная блокировка TX на загрузке |
| R1, R2 | 2 | 1 кОм, SMD 0805 | Последовательные TX/RX |
| R3 | 1 | 10 кОм SMD 0805 | Pull-up U2 /OE к 3V3 |
| C3 | 1 | 100 нФ X7R ≥16 В | Развязка U2 3V3 |
| C1 | 1 | 100 нФ X7R, ≥16 В, 0805 | Развязка VCC 3,3 В |
| C2 | 1 | 100 нФ X7R, ≥50 В, 0805 | Развязка VS 12 В |
| J1 | 1 | 3-контактный разъём провода (ACC / GND / K) | Выбрать тип и шаг до PCB |
| J2 | 1 | 6-контактный разъём к пигтейлу дисплея | Штатный 12PIN шлейф разделывают по сигналам |
| J3 | 1 | 2-контактный разъём 5 В / GND | От отдельного автомобильного DC/DC |
| JP1 | 1 | Сервисная перемычка 2pin с колпачком | Отключение только ветви БК от K-Line |
| F1 | 1 | Держатель + предохранитель 1 А | В самом начале отвода питания ACC |
| PWR | 1 | Защищённый автомобильный преобразователь 12→5 В с запасом не менее 2 А | Нужны документы по пуску/помехам/импульсам, модель не утверждена |
| WIRE | по месту | Автомобильные жилы ~0,35–0,5 мм², термоусадка, изоляция, пигтейл WAVESHARE | Цвета назначены в wiring |

**Удалены из R0:** 5-вольтовый буфер SN74AHCT1G125 и делитель 10k/18k. В R1B применяется **другой** буфер **SN74LVC1G125-Q1 3,3 В**, предназначенный для безопасного запрета TX во время загрузки UART0, не для согласования уровней.

## Ссылки

| Что искать | OZON | Wildberries | AliExpress | Документация |
|---|---|---|---|---|
| Waveshare 2.1 ESP32-S3 | [Кандидат 5365894816](https://www.ozon.ru/product/waveshare-esp32-s3-2-1inch-round-ips-capacitive-touch-display-board-480-480-262k-color-wifi-5365894816/) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=Waveshare%20ESP32-S3%202.1) | [Поиск](https://aliexpress.ru/wholesale?SearchText=ESP32-S3-Touch-LCD-2.1) | [Waveshare](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.1) |
| E-L9637D / L9637D | [Поиск](https://www.ozon.ru/search/?text=L9637D) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=L9637D) | [Поиск](https://aliexpress.ru/wholesale?SearchText=L9637D) | [Производитель ST](https://www.st.com/en/automotive-analog-and-power/l9637.html) |
| Буфер U2 SN74LVC1G125-Q1 | [Поиск](https://www.ozon.ru/search/?text=SN74LVC1G125) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=SN74LVC1G125) | [Поиск](https://aliexpress.ru/wholesale?SearchText=SN74LVC1G125) | [TI](https://www.ti.com/product/SN74LVC1G125-Q1) |
| SOIC-8→DIP8 (на макет) | [Поиск](https://www.ozon.ru/search/?text=SOIC8%20DIP8%20переходник) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=SOIC8%20DIP8) | [Поиск](https://aliexpress.ru/wholesale?SearchText=SOIC8%20DIP8) | Сверить шаг и корпус |
| Резисторы 0805 / C 100 нФ | [Поиск](https://www.ozon.ru/search/?text=0805%201k%20100nF) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=0805%201k%20100nF) | [Поиск](https://aliexpress.ru/wholesale?SearchText=0805%201k%20100nF) | Важны допуски и напряжение C2 |
| SH1.0 12 pin кабель | [Поиск](https://www.ozon.ru/search/?text=SH1.0%2012pin) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=SH1.0%2012pin) | [Поиск](https://aliexpress.ru/wholesale?SearchText=JST%20SH%201.0%2012pin) | Лучше штатный кабель Waveshare |
| Автомобильный DC/DC 5V ≥2A | [Поиск](https://www.ozon.ru/search/?text=автомобильный%20DC%20DC%205В%202А) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=DC%20DC%2012%205%202A) | [Поиск](https://aliexpress.ru/wholesale?SearchText=automotive%20DC-DC%205V%202A) | Не брать неподтверждённый модуль для постоянной установки |
| Держатель предохранителя 1А | [Поиск](https://www.ozon.ru/search/?text=держатель%20предохранителя%201А) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=держатель%20предохранителя%201А) | [Поиск](https://aliexpress.ru/wholesale?SearchText=automotive%20inline%20fuse%20holder) | Отдельная цепь ACC |

Ссылки, помеченные **«Поиск»**, открывают поиск, а не подтверждённый остаток или товар. На момент проверки ST обозначает E-L9637D действующей автомобильной микросхемой SO-8: [официальная страница](https://www.st.com/en/automotive-analog-and-power/l9637.html). Не покупать готовый USB KKL в расчёте на подключение к UART GPIO напрямую.

**Потребуется позже:** детали печатной платы по KiCad BOM (конкретные разъёмы/посадочные места), прошивка firmware.bin и измеренный корпус. Точная цена и наличие не подтверждены.
