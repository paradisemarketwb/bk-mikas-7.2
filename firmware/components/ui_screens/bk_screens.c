#include "bk_screens.h"
const bk_screen_t bk_screens[5] = {
 {"ГЛАВНЫЙ ПРИБОР", BK_SPEED, BK_RPM, BK_VOLTAGE},
 {"ДВИГАТЕЛЬ", BK_COOLANT, BK_VOLTAGE, BK_RPM},
 {"ТОПЛИВО", BK_FUEL_LPH, BK_RANGE, BK_INJECTION},
 {"ДАТЧИКИ", BK_MAF, BK_LAMBDA, BK_IGNITION},
 {"ПОЕЗДКА", BK_DISTANCE, BK_TRIP_SECONDS, BK_AVG_SPEED}
};
const char *const bk_metric_names[BK_METRIC_COUNT] = {
 "Обороты", "Температура ОЖ", "Напряжение ЭБУ", "Дроссель", "Расход топлива",
 "Расход воздуха", "УОЗ", "Впрыск", "Скорость", "Запас хода", "Пробег",
 "Время поездки", "Средняя скорость", "Ошибки ЭБУ", "Лямбда raw"};
const char *const bk_metric_units[BK_METRIC_COUNT] = {
 "об/мин", "°C", "В", "%", "л/ч", "кг/ч", "°", "мс", "км/ч", "км", "км", "с", "км/ч", "", "raw"};
const double bk_metric_max[BK_METRIC_COUNT] = {6000,120,16,100,30,200,60,20,160,800,1000,86400,160,20,255};
