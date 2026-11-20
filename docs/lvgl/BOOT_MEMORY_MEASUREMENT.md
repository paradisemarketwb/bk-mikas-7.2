# Измерение удаления заставки

Контрольный эксперимент выполнен на Linux в native LVGL 9.6.0, исходный
Classic R2 commit `7f15da7`. Чтобы не смешивать эффект заставки с новыми
шрифтами, моделями и источниками Hunter, использован один старый бинарник
с двумя вариантами перехода: прежний `bk_ui_boot_start(1)` и прямой
`bk_ui_select_screen(1)`. Полный исходный проект Hunter имеет другие ресурсы;
его размер не используется как мера экономии заставки.

В старый simulator/main.c временного detached worktree добавлены два CLIфлага
`--measure-boot` и `--measure-immediate`. Сразу после `bk_ui_create`/sample
снималось состояние, затем выполнялся выбранный переход, 20ms LVGLtick/handler
и ещё49шагов по20ms. Измерение — `lv_mem_monitor`, рекурсивный подсчёт объектов
`lv_screen_active` и число таймеров через `lv_timer_get_next`.

| Режим | Момент | Использовано LVGL heap, bytes | Пиковое использование, bytes | Объектов | Таймеров |
|---|---|---:|---:|---:|---:|
| Оба | До перехода |15424|15424|16|3|
| С заставкой |20ms|13528|94224|13|4|
| С заставкой |1000ms|15136|99320|16|3|
| Прямой переход |20ms|15136|99320|16|3|
| Прямой переход |1000ms|15136|99320|16|3|

**Измеренная экономия пикового/установившегося LVGL heap —0байт.** Удаление
заставки убирает дополнительный временный таймер и600ms ожидания; оно не даёт
права заявлять уменьшение общего RAMпика. Основной прибор и его rendercache
занимают больше временного приветствия и определяют пик в обоих случаях.
Три фоновых таймера остаются общими для рендера/ввода/истории.

Для воспроизведения использовать старый commit в отдельной рабочей копии,
тот же simulator/lv_conf.h (встроенный LVGL heap2MiB) и следующие функции:

```c
static unsigned object_count(lv_obj_t *o) {
    unsigned n = 1;
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++)
        n += object_count(lv_obj_get_child(o, i));
    return n;
}
static void memory_line(const char *stage) {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    unsigned timers = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t))
        timers++;
    printf("%s used=%zu peak=%zu objects=%u timers=%u\n", stage,
           m.total_size - m.free_size, m.max_used,
           object_count(lv_screen_active()), timers);
}
```

Вызывать memory_line до перехода, после одного шага20ms и после ещё49шагов.
Каждый вариант запускать в новом процессе. Эта временная инструментальная
правка не включена в runtime Hunter и не меняет main репозитория.

Это измерение native allocator, не ESP32 heap. В прошивке используется другой
allocator/драйвер; физические цифры памяти, FPS и влияние PSRAM ещё не измерены.
Для них добавлен CONFIG_BK_PROFILE_UI с логом каждые10секунд.
