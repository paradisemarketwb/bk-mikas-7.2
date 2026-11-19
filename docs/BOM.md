# Что купить для R2

Полные таблицы конкретных MPN/посадочных мест — [BOM интерфейса](../hardware/kicad/r2/BOM.csv)
и [проект питания](../hardware/kicad/power-r2/README.md). Корпуса и крепёж —
[CAD R2](../hardware/case/r2/README.md). Замена MPN требует проверки pinout,
package, напряжений, температуры и импульсных режимов, а не только похожего имени.

Основные компоненты:

| Деталь | Где проверять характеристики |
|---|---|
|Waveshare ESP32-S3-Touch-LCD-2.1|[Официальная страница](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.1)|
|L9637D SOIC8|[ST datasheet](https://www.st.com/resource/en/datasheet/l9637.pdf)|
|SN74LVC1G125QDBVRQ1|[TI](https://www.ti.com/product/SN74LVC1G125-Q1)|
|2N7002 SOT23|Проверить выводыG1/S2/D3 по MPN изBOM|
|LM74800QDRRRQ1 и силовые MOSFET/TVS|[TI datasheet](https://www.ti.com/lit/ds/symlink/lm7480-q1.pdf), [расчёт питания](../hardware/kicad/power-r2/README.md)|
|RECOM R-78B5.0-2.0|[Официальный datasheet](https://recom-power.com/pdf/Innoline/R-78B-2.0.pdf)|
|LM66100|[TI](https://www.ti.com/product/LM66100)|
|TPS2553DBVR, ограничитель выхода5В|[TI](https://www.ti.com/product/TPS2553)|

Указанные ниже адреса — **поисковые ссылки**, а не подтверждённые карточки
товара. Наличие, цена, подлинность и ревизия платы не проверялись.

- [OZON: Waveshare ESP32-S3 Touch LCD2.1](https://www.ozon.ru/search/?text=Waveshare%20ESP32-S3-Touch-LCD-2.1)
- [Wildberries: Waveshare ESP32-S3](https://www.wildberries.ru/catalog/0/search.aspx?search=Waveshare%20ESP32-S3)
- [AliExpress: точное имя модуля](https://www.aliexpress.com/w/wholesale-ESP32-S3-Touch-LCD-2.1.html)
- [OZON: L9637D](https://www.ozon.ru/search/?text=L9637D)
- [AliExpress: L9637D](https://www.aliexpress.com/w/wholesale-L9637D.html)
- [Mouser: L9637D](https://www.mouser.com/c/?q=L9637D)
- [DigiKey: LM74800QDRRRQ1](https://www.digikey.com/en/products/result?keywords=LM74800QDRRRQ1)
- [Mouser: R-78B5.0-2.0](https://www.mouser.com/c/?q=R-78B5.0-2.0)

Не заменяйте силовую плату случайным автомобильным buck-модулем. Она выполняет
reverse/OV/UV/load-dump и USB ORing. Керамика, резисторы и защитные диоды имеют
указанные напряжения/импульсные требования; снижать их без расчёта нельзя.

Для ручной сборки нужны паяльная станция, флюс, увеличение, мультиметр,
лабораторный источник с ограничением тока и осциллограф для проверки запуска.
QFN силового контроллера требует корректной пайки exposed pad и осмотра;
при отсутствии оборудования разумнее заказать монтаж у изготовителя.
