# Закупка: дисплей, K-Line, питание, корпус

Поиск 16.11.2026. Ссылки на маркетплейсы со словом **поиск** — именно страницы поиска, не подтверждённые товары. Не подменять надпись «K-Line USB» на «UART»: преобразователь USB требует драйвера USB Host и **не** подключается напрямую к 4-пиновому UART.

## Готовый преобразователь L9637D — что обнаружено

**Пока не найден подтверждённый к покупке на OZON/WB/AliExpress именно готовый компактный модуль K-Line↔UART TTL 3,3 В с опубликованной распиновкой, защитой и питанием.** Не покупаем неизвестную плату по одному слову K-Line. Варианты, которые реально документированы:

- [CompactTool USB–K-Line FT232RL+L9637D](https://compacttool.ru/preobrazovatel-interfejsa-usb---k-line) — **готовый USB**, 1 800 ₽ в карточке при проверке. **Не подходит для прямого соединения с GPIO43/GPIO44**; возможен только как USB-диагностика или донор после инженерной проверки.
- [OZON USB–K-Line FT232RL+L9637D, арт. 5157351977](https://www.ozon.ru/product/programmator-adapter-usb-k-line-so-vstroennoy-galvanicheskoy-razvyazkoy-ft232rl-l9637d-dlya-5157351977/) — готовый **USB-адаптер**, не UART-модуль; цена/наличие зависят от продавца.
- [O-B-D: плата с L9637D для Arduino](https://o-b-d.ru/adapter-k-line-dlya-arduino-i-radioconstructor-ram226-usb-adapter-linii-k-l/) — пример сборки с описанием, не подтверждение поставки готового изделия.

Кандидат готового UART-модуля должен иметь подписанные выводы **K, +12/VS, GND, +5/VCC, TX, RX** и документированное преобразование **3,3↔5 В**. Если у него только USB-C/FT232 — это не тот модуль.

## Детали для компактной платы на L9637D (BOM)

| Поз. | Кол-во | Назначение |
|---|---:|---|
| U1 L9637D / E-L9637D, SOIC-8 | 1 | автомобильный интерфейс K-Line ISO9141 |
| U2 SN74AHCT1G125, SOT-23-5 | 1 | буфер ESP32 TX 3,3→5 В |
| R1 10 кОм, R2 18 кОм | 1+1 | делитель с RX L9637D 5→3,21 В |
| R3 47 кОм | 1 | подтяжка входа буфера к 3,3 В |
| C1–C3 100 нФ X7R | 3 | развязка 5 В и VS |
| J1 | 1 | автомобильные +12 ACC / GND / K-Line |
| J2 | 1 | 4-pin UART разъём совместимый с Waveshare SH1.0, плюс питание 5 В отдельной линией |
| Адаптер SOIC-8→DIP8 | 1 (опц.) | макетирование без изготовления печатной платы |
| Защита входа +12 и DC/DC 5В/2А | 1 комплект | предохранитель, переполюсовка, TVS/фильтрация и стабилизация |

## Где купить / искать

| Компонент | OZON | WB | AliExpress | Дополнительно |
|---|---|---|---|---|
| Круглый сенсорный дисплей Waveshare 2,1 480×480 | [Товар, арт. 5365894816](https://www.ozon.ru/product/waveshare-esp32-s3-2-1inch-round-ips-capacitive-touch-display-board-480-480-262k-color-wifi-5365894816/) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=Waveshare%20ESP32-S3%202.1) | [Поиск](https://aliexpress.ru/wholesale?SearchText=ESP32-S3-Touch-LCD-2.1) | [Официальная Waveshare](https://www.waveshare.com/esp32-s3-touch-lcd-2.1.htm) |
| L9637D / E-L9637D | [Поиск](https://www.ozon.ru/search/?text=L9637D) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=L9637D) | [Поиск](https://aliexpress.ru/wholesale?SearchText=L9637D) | [LCSC E-L9637D](https://www.lcsc.com/product-detail/rf-transceiver-ics_stmicroelectronics-e-l9637d_C1543935.html) · [каталог цен](https://www.rlocman.ru/comp/shop.html?q=L9637D) |
| SN74AHCT1G125 | [Поиск](https://www.ozon.ru/search/?text=SN74AHCT1G125) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=SN74AHCT1G125) | [Поиск](https://aliexpress.ru/wholesale?SearchText=SN74AHCT1G125) | [ChipDip поиск](https://www.chipdip.ru/search?searchtext=SN74AHCT1G125DBVR) |
| SOIC-8→DIP8 | [Поиск](https://www.ozon.ru/search/?text=SOIC8%20DIP8%20переходник) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=SOIC8%20DIP8) | [Поиск](https://aliexpress.ru/wholesale?SearchText=SOIC8%20DIP8%20adapter) | Проверить шаг и ширину корпуса |
| DC/DC фикс. 5В 2А, 4.5–40 В | [Товар-кандидат, арт. 3189360720](https://www.ozon.ru/product/dc-dc-fiksirovannyy-preobrazovatel-vhod-4-5v-40v-vyhod-5v-tok-2a-1-usb-s-indikatorom-3189360720/) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=DC%20DC%2012%205%202A) | [Поиск](https://aliexpress.ru/wholesale?SearchText=automotive%20buck%205V%202A) | Только стенд до тестов на автомобильные помехи |
| Провода SH1.0 4pin и 12pin | [Поиск](https://www.ozon.ru/search/?text=SH1.0%204pin%2012pin) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=SH1.0%204pin) | [Поиск](https://aliexpress.ru/wholesale?SearchText=JST%20SH%201.0%204pin%2012pin) | В комплект Waveshare обычно входят штатные провода |
| 1A предохранитель, держатель, провода 0,35–0,5 мм², разъём 3-pin | [Поиск](https://www.ozon.ru/search/?text=держатель%20предохранителя%201A%20superseal) | [Поиск](https://www.wildberries.ru/catalog/0/search.aspx?search=superseal%203pin) | [Поиск](https://aliexpress.ru/wholesale?SearchText=superseal%203pin%20fuse%20holder) | Проверить влагозащиту и фиксацию контактов |

**Не покупай сразу партию плат:** сначала подтвердить питание модуля Waveshare, правильную разводку и размер корпуса. Цены и наличие меняются. Ни готовая UART-плата, ни изготовленная PCB здесь ещё не заявляются как испытанные.

Технические первоисточники: [Waveshare](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.1), [ST L9637](https://www.st.com/en/automotive-analog-and-power/l9637.html).
