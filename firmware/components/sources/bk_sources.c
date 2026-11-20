#include "bk_sources.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(char *reason, size_t cap, const char *why) {
  if (reason && cap)
    snprintf(reason, cap, "%s", why);
  return false;
}
static bool bounded(double v, double lo, double hi) {
  return isfinite(v) && v >= lo && v <= hi;
}
static bool name_valid(const char *s, size_t size) {
  const char *end = memchr(s, 0, size);
  if (!end || end == s)
    return false;
  for (const char *p = s; p < end; p++)
    if ((unsigned char)*p < 32 || (unsigned char)*p == 127)
      return false;
  return true;
}
void bk_calibration_defaults(bk_calibration_t *c, bk_calibration_kind_t kind) {
  if (!c)
    return;
  memset(c, 0, sizeof(*c));
  c->kind = kind;
  c->method = BK_CAL_RAW;
  c->unit = kind == BK_CAL_MAF ? BK_UNIT_G_S : BK_UNIT_V;
  c->k = 1;
  c->divisor = 1;
  c->raw_max = kind == BK_CAL_MAF ? 65535 : 255;
  c->output_max = kind == BK_CAL_MAF ? 5000 : 5;
  snprintf(c->profile_name, sizeof(c->profile_name), "%s",
           kind == BK_CAL_MAF ? "user-maf" : "user-lambda");
}
bool bk_calibration_validate(const bk_calibration_t *c, char *reason,
                             size_t cap) {
  if (reason && cap)
    reason[0] = 0;
  if (!c)
    return fail(reason, cap, "Нет конфигурации");
  if (c->kind < BK_CAL_MAF || c->kind > BK_CAL_LAMBDA ||
      c->method < BK_CAL_RAW || c->method > BK_CAL_TABLE)
    return fail(reason, cap, "Неизвестный тип преобразования");
  if (!name_valid(c->profile_name, sizeof(c->profile_name)))
    return fail(reason, cap, "Некорректное имя профиля");
  if (!bounded(c->k, -1e6, 1e6) || !bounded(c->b, -1e9, 1e9) ||
      !bounded(c->divisor, 1e-9, 1e9))
    return fail(reason, cap, "Неконечные коэффициенты или неверный делитель");
  if (!bounded(c->raw_min, -1e9, 1e9) || !bounded(c->raw_max, -1e9, 1e9) ||
      c->raw_min >= c->raw_max)
    return fail(reason, cap, "Неверный диапазон raw");
  if (!bounded(c->output_min, 0, 1e9) || !bounded(c->output_max, 0, 1e9) ||
      c->output_min >= c->output_max)
    return fail(reason, cap, "Неверный выходной диапазон");
  if (c->lambda_kind < BK_LAMBDA_UNSPECIFIED ||
      c->lambda_kind > BK_LAMBDA_WIDEBAND)
    return fail(reason, cap, "Неизвестный тип лямбда-зонда");
  if (c->kind == BK_CAL_MAF) {
    if (c->unit != BK_UNIT_G_S && c->unit != BK_UNIT_KG_H)
      return fail(reason, cap, "ДМРВ: нужны г/с или кг/ч");
    if (c->output_max > (c->unit == BK_UNIT_G_S ? 20000 : 72000))
      return fail(reason, cap, "Превышен допустимый диапазон ДМРВ");
  } else {
    if (c->unit != BK_UNIT_V && c->unit != BK_UNIT_MV &&
        c->unit != BK_UNIT_LAMBDA_RATIO && c->unit != BK_UNIT_AFR)
      return fail(reason, cap, "Несовместимая единица лямбда-зонда");
    if (c->configured && c->lambda_kind == BK_LAMBDA_UNSPECIFIED)
      return fail(reason, cap, "Укажите тип лямбда-зонда");
    if ((c->unit == BK_UNIT_LAMBDA_RATIO || c->unit == BK_UNIT_AFR) &&
        c->lambda_kind != BK_LAMBDA_WIDEBAND)
      return fail(reason, cap,
                  "Lambda/AFR требуют явного широкополосного профиля");
    double max = c->unit == BK_UNIT_V     ? 20
                 : c->unit == BK_UNIT_MV  ? 20000
                 : c->unit == BK_UNIT_AFR ? 100
                                          : 10;
    if (c->output_max > max)
      return fail(reason, cap, "Превышен диапазон выбранных единиц");
  }
  if (c->configured && c->method == BK_CAL_RAW)
    return fail(reason, cap,
                "Для расчёта выберите линейный профиль или таблицу");
  if (c->point_count > BK_CALIBRATION_POINTS)
    return fail(reason, cap, "Таблица содержит больше восьми точек");
  if (c->method == BK_CAL_TABLE && c->point_count < 2)
    return fail(reason, cap, "Таблице нужны минимум две точки");
  for (unsigned i = 0; i < c->point_count; i++) {
    if (!bounded(c->points[i].raw, c->raw_min, c->raw_max) ||
        !bounded(c->points[i].value, c->output_min, c->output_max) ||
        (i && c->points[i].raw <= c->points[i - 1].raw))
      return fail(reason, cap, "Точки вне диапазона или raw не возрастает");
  }
  return true;
}
static bool convert(double in, bk_unit_t from, bk_unit_t to, double *out) {
  if (from == to) {
    *out = in;
    return true;
  }
  if (from == BK_UNIT_G_S && to == BK_UNIT_KG_H) {
    *out = in * 3.6;
    return true;
  }
  if (from == BK_UNIT_KG_H && to == BK_UNIT_G_S) {
    *out = in / 3.6;
    return true;
  }
  if (from == BK_UNIT_V && to == BK_UNIT_MV) {
    *out = in * 1000;
    return true;
  }
  if (from == BK_UNIT_MV && to == BK_UNIT_V) {
    *out = in / 1000;
    return true;
  }
  return false;
}
bk_calibration_result_t bk_calibration_evaluate(const bk_calibration_t *c,
                                                double raw, bk_unit_t target,
                                                double *out) {
  if (!out)
    return BK_CAL_INVALID_CONFIG;
  *out = NAN;
  if (!bk_calibration_validate(c, NULL, 0))
    return BK_CAL_INVALID_CONFIG;
  if (!isfinite(raw))
    return BK_CAL_NONFINITE;
  if (raw < c->raw_min || raw > c->raw_max)
    return BK_CAL_RAW_OUT_OF_RANGE;
  if (target == BK_UNIT_RAW) {
    *out = raw;
    return BK_CAL_OK;
  }
  if (!c->configured || c->method == BK_CAL_RAW)
    return BK_CAL_NEEDS_CONFIG;
  double value;
  if (c->method == BK_CAL_LINEAR)
    value = raw * c->k / c->divisor + c->b;
  else {
    if (raw < c->points[0].raw || raw > c->points[c->point_count - 1].raw)
      return BK_CAL_RAW_OUT_OF_RANGE;
    unsigned upper = 1;
    while (upper < c->point_count - 1 && raw > c->points[upper].raw)
      upper++;
    bk_calibration_point_t a = c->points[upper - 1], b = c->points[upper];
    value = a.value + (raw - a.raw) / (b.raw - a.raw) * (b.value - a.value);
  }
  if (!isfinite(value))
    return BK_CAL_NONFINITE;
  if (value < c->output_min || value > c->output_max)
    return BK_CAL_OUTPUT_OUT_OF_RANGE;
  double converted;
  if (!convert(value, c->unit, target, &converted))
    return BK_CAL_INVALID_UNIT;
  if (!isfinite(converted))
    return BK_CAL_NONFINITE;
  *out = converted;
  return BK_CAL_OK;
}
const char *bk_unit_name(bk_unit_t u) {
  switch (u) {
  case BK_UNIT_RAW:
    return "raw";
  case BK_UNIT_G_S:
    return "g/s";
  case BK_UNIT_KG_H:
    return "kg/h";
  case BK_UNIT_V:
    return "V";
  case BK_UNIT_MV:
    return "mV";
  case BK_UNIT_LAMBDA_RATIO:
    return "lambda";
  case BK_UNIT_AFR:
    return "AFR";
  case BK_UNIT_NATIVE:
    return "native";
  default:
    return "unknown";
  }
}

void bk_sources_init(bk_sources_t *r) {
  if (r)
    memset(r, 0, sizeof(*r));
}
static bool metric_valid(const char *id) {
  if (!id)
    return false;
  size_t n = 0;
  for (; n < BK_METRIC_ID_SIZE && id[n]; n++) {
    char c = id[n];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
          c == '_' || c == '-'))
      return false;
  }
  return n > 0 && n < BK_METRIC_ID_SIZE;
}
static bk_source_entry_t *find(bk_sources_t *r, const char *id,
                               bk_source_id_t source) {
  if (!r || !metric_valid(id))
    return NULL;
  for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++)
    if (r->entries[i].used && r->entries[i].config.source == source &&
        !strcmp(r->entries[i].config.metric, id))
      return &r->entries[i];
  return NULL;
}
bool bk_sources_register(bk_sources_t *r, const bk_source_config_t *c) {
  if (!r || !c || !memchr(c->metric, 0, sizeof(c->metric)) ||
      !metric_valid(c->metric) || c->source <= BK_SOURCE_NONE ||
      c->source >= BK_SOURCE_COUNT || !c->fresh_ms || !c->confirm_samples ||
      c->confirm_samples > 1000 || !isfinite(c->minimum) ||
      !isfinite(c->maximum) || c->minimum >= c->maximum ||
      c->unit < BK_UNIT_RAW || c->unit > BK_UNIT_NATIVE)
    return false;
  bk_source_entry_t *e = find(r, c->metric, c->source);
  if (e)
    return false;
  bk_source_entry_t *target = NULL;
  /* A disconnected selected source retains its identity: arriving unrelated
   * adapters must not silently replace it when fallback is disabled. */
  for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++) {
    e = &r->entries[i];
    if (!e->used && e->selected && e->config.source == c->source &&
        !strcmp(e->config.metric, c->metric)) {
      target = e;
      break;
    }
  }
  if (!target)
    for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++)
      if (!r->entries[i].used && !r->entries[i].selected) {
        target = &r->entries[i];
        break;
      }
  if (!target)
    return false;
  bool selected = target->selected;
  memset(target, 0, sizeof(*target));
  target->used = true;
  target->selected = selected;
  target->config = *c;
  target->status = BK_SOURCE_NO_SIGNAL;
  target->sample.value = NAN;
  target->sample.source = c->source;
  target->sample.unit = c->unit;
  return true;
}
bool bk_sources_remove(bk_sources_t *r, const char *id, bk_source_id_t source) {
  bk_source_entry_t *e = find(r, id, source);
  if (!e)
    return false;
  if (e->selected) {
    e->used = false;
    e->published = false;
    e->confirmations = 0;
    e->status = BK_SOURCE_NO_SIGNAL;
  } else
    memset(e, 0, sizeof(*e));
  return true;
}

bool bk_sources_publish(bk_sources_t *r, const char *id,
                        const bk_source_sample_t *s) {
  if (!s)
    return false;
  bk_source_entry_t *e = find(r, id, s->source);
  if (!e)
    return false;
  if (e->published && s->at_ms <= e->sample.at_ms)
    return false;
  if (e->published && s->at_ms - e->sample.at_ms > e->config.fresh_ms)
    e->confirmations = 0;
  e->sample = *s;
  e->sample.simulation = s->simulation || s->source == BK_SOURCE_SIMULATION;
  e->sample.profile_name[sizeof(e->sample.profile_name) - 1] = 0;
  e->published = true;
  if (!isfinite(s->value) || (s->raw_valid && !isfinite(s->raw)) ||
      s->unit != e->config.unit) {
    e->status = BK_SOURCE_INVALID;
    e->confirmations = 0;
    return false;
  }
  if (s->value < e->config.minimum || s->value > e->config.maximum) {
    e->status = BK_SOURCE_OUT_OF_RANGE;
    e->confirmations = 0;
    return false;
  }
  if (!s->confirmed) {
    e->status = BK_SOURCE_UNCONFIRMED;
    e->confirmations = 0;
    return true;
  }
  if (e->confirmations < e->config.confirm_samples)
    e->confirmations++;
  e->status = e->confirmations >= e->config.confirm_samples
                  ? BK_SOURCE_VALID
                  : BK_SOURCE_UNCONFIRMED;
  return true;
}
const char *bk_source_status_reason(bk_source_status_t s) {
  switch (s) {
  case BK_SOURCE_VALID:
    return "Данные подтверждены";
  case BK_SOURCE_NO_SIGNAL:
    return "Нет сигнала";
  case BK_SOURCE_STALE:
    return "Данные устарели";
  case BK_SOURCE_UNCONFIRMED:
    return "Ожидается подтверждение корректности источника";
  case BK_SOURCE_OUT_OF_RANGE:
    return "Значение вне допустимого диапазона";
  case BK_SOURCE_SIMULATION_BLOCKED:
    return "Симуляция не разрешена в рабочем режиме";
  default:
    return "Некорректные данные источника";
  }
}
static bk_source_status_t state(const bk_source_entry_t *e,
                                const bk_source_policy_t *p, uint64_t now) {
  if (!e || !e->published)
    return BK_SOURCE_NO_SIGNAL;
  if (e->sample.simulation && !p->allow_simulation)
    return BK_SOURCE_SIMULATION_BLOCKED;
  if (now < e->sample.at_ms)
    return BK_SOURCE_INVALID;
  if (now - e->sample.at_ms > e->config.fresh_ms)
    return BK_SOURCE_STALE;
  return e->status;
}
bk_source_selection_t bk_sources_select(bk_sources_t *r, const char *id,
                                        const bk_source_policy_t *policy,
                                        uint64_t now) {
  bk_source_selection_t out = {
      .sample = {.value = NAN, .source = BK_SOURCE_NONE},
      .status = BK_SOURCE_NO_SIGNAL,
      .reason = "Нет зарегистрированного источника"};
  if (!r || !metric_valid(id))
    return out;
  const bk_source_policy_t def = {0};
  const bk_source_policy_t *p = policy ? policy : &def;
  if (p->preferred < BK_SOURCE_NONE || p->preferred >= BK_SOURCE_COUNT) {
    out.status = BK_SOURCE_INVALID;
    out.reason = bk_source_status_reason(out.status);
    return out;
  }
  bk_source_entry_t *chosen = NULL, *best = NULL, *first = NULL;
  for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++) {
    bk_source_entry_t *e = &r->entries[i];
    if ((!e->used && !e->selected) || strcmp(e->config.metric, id))
      continue;
    if (e->used && (!first || e->config.priority > first->config.priority))
      first = e;
    if ((p->preferred != BK_SOURCE_NONE && e->config.source == p->preferred) ||
        (p->preferred == BK_SOURCE_NONE && e->selected))
      chosen = e;
    if (e->used && state(e, p, now) == BK_SOURCE_VALID &&
        (!best || e->config.priority > best->config.priority ||
         (e->config.priority == best->config.priority &&
          e->config.source < best->config.source)))
      best = e;
  }
  if (p->preferred == BK_SOURCE_NONE && !chosen)
    chosen = best ? best : first;
  if (p->auto_fallback) {
    if (!chosen || state(chosen, p, now) != BK_SOURCE_VALID)
      chosen = best ? best : chosen;
    else if (p->preferred == BK_SOURCE_NONE && best)
      chosen = best;
  }
  if (!chosen)
    return out;
  if (state(chosen, p, now) == BK_SOURCE_VALID || chosen->selected ||
      p->preferred != BK_SOURCE_NONE)
    for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++)
      if (!strcmp(r->entries[i].config.metric, id))
        r->entries[i].selected = &r->entries[i] == chosen;
  out.sample = chosen->sample;
  out.sample.source = chosen->config.source;
  out.sample.unit = chosen->config.unit;
  out.status = state(chosen, p, now);
  out.reason = bk_source_status_reason(out.status);
  /* Never turn absence into a believable zero, while keeping raw/time/source.
   */
  if (out.status != BK_SOURCE_VALID)
    out.sample.value = NAN;
  return out;
}
const char *bk_source_name(bk_source_id_t s) {
  static const char *const n[] = {"none",         "ecu",
                                  "gps",          "speed-sensor",
                                  "fuel-sensor",  "outside-temperature",
                                  "oil-pressure", "oil-temperature",
                                  "tpms",         "parking",
                                  "analog",       "digital",
                                  "fuel-mode",    "derived",
                                  "user",         "module",
                                  "simulation"};
  return s >= BK_SOURCE_NONE && s < BK_SOURCE_COUNT ? n[s] : "unknown";
}
bool bk_sources_publish_simulation(bk_sources_t *r, const char *id,
                                   double value, bk_unit_t unit, uint64_t now) {
  bk_source_sample_t s = {.value = value,
                          .source = BK_SOURCE_SIMULATION,
                          .unit = unit,
                          .at_ms = now,
                          .confirmed = true,
                          .simulation = true};
  return bk_sources_publish(r, id, &s);
}

void bk_signal_activity_init(bk_signal_activity_t *a) {
  if (a)
    memset(a, 0, sizeof(*a));
}
bool bk_signal_activity_update(bk_signal_activity_t *a, double value,
                               uint64_t at) {
  if (!a || !isfinite(value))
    return false;
  if (a->count) {
    unsigned last = (a->next + BK_ACTIVITY_POINTS - 1) % BK_ACTIVITY_POINTS;
    if (at <= a->points[last].at_ms)
      return false;
  }
  a->points[a->next] = (bk_signal_point_t){value, at};
  a->next = (a->next + 1) % BK_ACTIVITY_POINTS;
  if (a->count < BK_ACTIVITY_POINTS)
    a->count++;
  return true;
}
bk_activity_result_t bk_signal_activity_get(const bk_signal_activity_t *a,
                                            uint64_t now, uint32_t window,
                                            double epsilon, double level) {
  bk_activity_result_t r = {
      .state = BK_ACTIVITY_NO_SIGNAL, .minimum = NAN, .maximum = NAN};
  if (!a || !a->count || !window || !isfinite(epsilon) || epsilon < 0 ||
      !isfinite(level))
    return r;
  unsigned start =
      (a->next + BK_ACTIVITY_POINTS - a->count) % BK_ACTIVITY_POINTS;
  double prev = 0;
  for (unsigned j = 0; j < a->count; j++) {
    bk_signal_point_t pt = a->points[(start + j) % BK_ACTIVITY_POINTS];
    if (pt.at_ms > now || now - pt.at_ms > window)
      continue;
    if (!r.samples)
      r.minimum = r.maximum = pt.value;
    else {
      if (pt.value < r.minimum)
        r.minimum = pt.value;
      if (pt.value > r.maximum)
        r.maximum = pt.value;
      if (fabs(pt.value - prev) > epsilon)
        r.changes++;
      if ((prev < level && pt.value >= level) ||
          (prev >= level && pt.value < level))
        r.crossings++;
    }
    prev = pt.value;
    r.samples++;
    r.last_ms = pt.at_ms;
  }
  if (!r.samples) {
    r.state = BK_ACTIVITY_STALE;
    return r;
  }
  r.state = r.changes ? BK_ACTIVITY_CHANGING : BK_ACTIVITY_STATIC;
  return r;
}

/* This catalog describes implementation/dependency availability, not live
 * signal. Service entries are metadata only: this component sends no ECU
 * commands. */
#define F(id, name, unit, sources, section, status, reason)                    \
  {id, name, unit, sources, reason, section, status}
static const bk_function_t catalog[] = {
    F("rpm", "Обороты двигателя", "rpm", "ecu", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE,
      "Реализовано чтение monitor0B; актуальность проверяется отдельно"),
    F("coolant", "Температура охлаждающей жидкости", "°C", "ecu",
      BK_SECTION_DAILY, BK_FUNCTION_AVAILABLE, "Реализовано чтение ЭБУ"),
    F("intake", "Температура воздуха на впуске", "°C", "ecu", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE,
      "Доступно для поддерживаемого monitor0B; отсутствие ответа не заменяется "
      "нулём"),
    F("throttle", "Положение дросселя", "%", "ecu", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE, "Реализовано чтение ЭБУ"),
    F("air", "Массовый расход воздуха", "g/s;kg/h", "ecu+calibration",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_CALIBRATION,
      "Raw доступен; задайте свой профиль ДМРВ"),
    F("injection", "Длительность впрыска", "ms", "ecu", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE, "Реализовано чтение ЭБУ"),
    F("advance", "Угол опережения зажигания", "deg", "ecu", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE, "Реализовано чтение ЭБУ"),
    F("knock_retard", "Поправка УОЗ по детонации", "deg", "ecu",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_PROTOCOL,
      "PID и формула для конкретной прошивки не подтверждены"),
    F("lambda1", "Лямбда-зонд 1", "raw;V;mV", "ecu+calibration",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_CALIBRATION,
      "Задайте тип датчика и коэффициенты; заводской масштаб не выдумывается"),
    F("lambda2", "Лямбда-зонд 2", "raw;V;mV", "ecu+calibration",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_CALIBRATION,
      "Поддержка второго канала зависит от ЭБУ; требуется профиль"),
    F("fuel_trim", "Коррекция топливной смеси", "ratio", "ecu",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_PROTOCOL,
      "Семантика зависит от ПО ЭБУ"),
    F("idle_steps", "Положение РХХ", "steps", "ecu", BK_SECTION_DIAGNOSTICS,
      BK_FUNCTION_NEEDS_PROTOCOL, "Чтение этого параметра не подтверждено"),
    F("idle_target", "Целевые обороты холостого хода", "rpm", "ecu",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_PROTOCOL,
      "Чтение этого параметра не подтверждено"),
    F("fan_state", "Состояние вентилятора", "bool", "ecu;digital",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_PROTOCOL,
      "Не путать неподтверждённый статус с температурным предположением"),
    F("volts", "Напряжение бортовой сети", "V", "ecu;analog", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE,
      "Чтение напряжения ЭБУ реализовано; свой измеритель требует адаптера"),
    F("engine_hours", "Наблюдаемое время работы двигателя", "h",
      "derived:confirmed-rpm", BK_SECTION_DAILY, BK_FUNCTION_AVAILABLE,
      "Это наблюдаемое время БК, не счётчик всей жизни двигателя"),
    F("speed", "Текущая скорость", "km/h", "gps;speed-sensor", BK_SECTION_DAILY,
      BK_FUNCTION_NEEDS_SENSOR,
      "Нужен подтверждённый источник скорости и его адаптер; из оборотов "
      "скорость не выводится"),
    F("odometer", "Общий пробег БК", "km", "derived:speed", BK_SECTION_DAILY,
      BK_FUNCTION_PLANNED,
      "Интегратор и сохранение пробега требуют реализации и проверенной "
      "скорости"),
    F("trip_distance", "Суточный пробег", "km", "derived:speed",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужен интегратор поездки с обработкой пропусков сигнала"),
    F("fuel_per_100km", "Мгновенный расход топлива", "L/100km",
      "derived:fuel+speed", BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужны подтверждённые расход и скорость, защита деления на ноль"),
    F("fuel", "Расход топлива в час", "L/h", "ecu", BK_SECTION_DAILY,
      BK_FUNCTION_AVAILABLE,
      "Реализована формула собственного monitor0B; проверить на своей прошивке "
      "ЭБУ"),
    F("average_fuel", "Средний расход топлива", "L/100km",
      "derived:fuel+distance", BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нет подтверждённого интегратора топлива и пробега"),
    F("fuel_used", "Израсходованное топливо", "L", "derived:fuel",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужен интегратор, учитывающий пропуски и смену топлива"),
    F("fuel_remaining", "Остаток топлива", "L", "fuel-sensor;user+derived",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Нужен ДУТ с адаптером либо явно обозначенная модель виртуального бака"),
    F("range", "Запас хода", "km", "derived:fuel-level+consumption",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Без достоверного остатка и расхода расчёт недоступен"),
    F("trip_time", "Время поездки и стоянок", "min", "derived:time+speed",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нет подтверждённого определения поездки и стоянки"),
    F("average_speed", "Средняя скорость", "km/h", "derived:distance+time",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужны достоверные пробег и длительность"),
    F("maximum_speed", "Максимальная скорость", "km/h", "derived:speed",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужен источник скорости и реализация статистики"),
    F("acceleration_100", "Разгон до 100 км/ч", "s", "derived:speed",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужны частота/точность источника скорости и правила измерения"),
    F("econometer", "Эконометр", "score", "derived:fuel+speed",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED, "Модель оценки не реализована"),
    F("trip_log", "Журнал поездок и заправок", "records",
      "derived;user;storage", BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Журнал поездок не подменяется существующим журналом событий ЭБУ"),
    F("trip_cost", "Стоимость поездки и топлива", "RUB", "derived;user",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужны интегратор расхода и пользовательская цена"),
    F("fuel_mode", "Учёт бензина и газа", "mode", "fuel-mode;user",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Нужны адаптер сигнала/явный ввод и отдельные профили расхода"),
    F("dtc_read", "Чтение ошибок ЭБУ", "codes", "ecu", BK_SECTION_DIAGNOSTICS,
      BK_FUNCTION_AVAILABLE, "Реализовано чтение ошибок без изменения ЭБУ"),
    F("dtc_decode", "Расшифровка ошибок", "text", "local-database",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_AVAILABLE,
      "Покрытие ограничено встроенной базой; неизвестные коды сохраняются"),
    F("ecu_identity", "Идентификация ЭБУ", "text", "ecu",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_PROTOCOL,
      "Не реализован подтверждённый запрос идентификации"),
    F("freeze_frame", "Стоп-кадр", "snapshot", "ecu;history",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_PLANNED,
      "Стоп-кадр ЭБУ и снимок БК требуют отдельной реализации/маркировки"),
    F("engine_graphs", "Графики двигателя", "samples", "history",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_AVAILABLE,
      "Доступны фактические ограниченные RAM-ряды; частота равна реальному "
      "опросу"),
    F("event_log", "Журнал диагностических событий", "events", "history",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_AVAILABLE,
      "Ограниченная история событий, без записи каждого изменения во Flash"),
    F("connection", "Состояние диагностической связи", "status",
      "ecu-transport", BK_SECTION_DIAGNOSTICS, BK_FUNCTION_AVAILABLE,
      "Состояние транспорта не доказывает корректность каждой метрики"),
    F("sources", "Доступность источников", "status", "source-registry",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_AVAILABLE,
      "Реестр хранит источник, raw, время, диапазон и подтверждение"),
    F("calibration", "Пользовательская калибровка", "profile", "user",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_AVAILABLE,
      "Линейный и табличный профили, без придуманных заводских коэффициентов"),
    F("warning_overheat", "Перегрев двигателя", "event", "coolant",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужны пользовательские пороги и проверенный движок предупреждений"),
    F("warning_voltage", "Низкое/высокое напряжение", "event", "volts;analog",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужны пороги, гистерезис и обработка устаревших значений"),
    F("warning_speed", "Превышение скорости", "event", "speed",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Нужны источник скорости и правила предупреждения"),
    F("warning_fuel", "Малый остаток топлива", "event",
      "fuel-sensor;virtual-tank", BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Остаток топлива должен иметь достоверный источник"),
    F("warning_warmup", "Необходимость прогрева", "event", "coolant",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нет подключённого правила прогрева"),
    F("warning_ice", "Вероятность гололёда", "event", "outside-temperature",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Нужны наружный датчик, адаптер и явно оценочное правило"),
    F("lights", "Включённые габариты или свет", "bool", "digital",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Требуются согласованный внешний вход и адаптер"),
    F("maintenance", "Напоминание о ТО", "event", "user;distance;hours;date",
      BK_SECTION_DAILY, BK_FUNCTION_PLANNED,
      "Нужна модель интервалов обслуживания"),
    F("audio", "Звуковые и голосовые сообщения", "audio", "buzzer;speaker",
      BK_SECTION_SETTINGS, BK_FUNCTION_PLANNED,
      "Наличие зуммера не означает реализованную систему уведомлений/голоса"),
    F("outside_temperature", "Наружная температура", "°C",
      "outside-temperature", BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "Драйвер и физический датчик не подключены; GPIO не назначены"),
    F("oil_pressure", "Давление масла", "bar", "oil-pressure", BK_SECTION_DAILY,
      BK_FUNCTION_NEEDS_SENSOR, "Нужны датчик, адаптер и калибровка"),
    F("oil_temperature", "Температура масла", "°C", "oil-temperature",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR, "Нужны датчик и адаптер"),
    F("tpms", "Давление в шинах", "bar", "tpms", BK_SECTION_DAILY,
      BK_FUNCTION_NEEDS_SENSOR,
      "TPMS-модуль и его протокол пока не подключены"),
    F("parking_front", "Передний парктроник", "cm", "parking", BK_SECTION_DAILY,
      BK_FUNCTION_NEEDS_SENSOR,
      "Нужны модуль и адаптер подтверждённых расстояний"),
    F("parking_rear", "Задний парктроник", "cm", "parking", BK_SECTION_DAILY,
      BK_FUNCTION_NEEDS_SENSOR,
      "Нужны модуль и адаптер подтверждённых расстояний"),
    F("gps_route", "Маршрут и координаты GPS", "coordinates", "gps",
      BK_SECTION_DAILY, BK_FUNCTION_NEEDS_SENSOR,
      "GPS-адаптер/приёмник не подключены"),
    F("custom_screens", "Настраиваемые экраны", "layout", "local-software",
      BK_SECTION_SETTINGS, BK_FUNCTION_AVAILABLE,
      "Настройки и общий renderer реализованы"),
    F("speed_calibration", "Калибровка скорости и пробега", "profile",
      "user;speed", BK_SECTION_SETTINGS, BK_FUNCTION_PLANNED,
      "Нет подключённого источника/интегратора скорости"),
    F("fuel_calibration", "Калибровка расхода и уровня топлива", "profile",
      "user;fuel", BK_SECTION_SETTINGS, BK_FUNCTION_PLANNED,
      "Не путать калибровку топлива с реализованной калибровкой ДМРВ"),
    F("oscilloscope", "Графический осциллограф", "waveform", "analog;module",
      BK_SECTION_DIAGNOSTICS, BK_FUNCTION_NEEDS_SENSOR,
      "K-Line-график не является осциллографом; нужны отдельный вход и "
      "адаптер"),
    F("update_backup", "Обновление ПО и резервное копирование", "image;json",
      "local-software", BK_SECTION_SETTINGS, BK_FUNCTION_AVAILABLE,
      "OTA и экспорт профиля реализованы; бинарные образы и JSON различаются"),
    F("service.dtc_clear", "Стирание ошибок", "command", "ecu",
      BK_SECTION_SERVICE, BK_FUNCTION_DISABLED,
      "Нет проверенной команды; отправка команд запрещена"),
    F("service.fan", "Тест вентилятора", "command", "ecu", BK_SECTION_SERVICE,
      BK_FUNCTION_DISABLED,
      "Нужны подтверждённая команда, условия, таймаут и безопасное завершение"),
    F("service.fuel_pump", "Тест бензонасоса", "command", "ecu",
      BK_SECTION_SERVICE, BK_FUNCTION_DISABLED,
      "Непроверенные исполнительные команды не отправляются"),
    F("service.injectors", "Тест форсунок", "command", "ecu",
      BK_SECTION_SERVICE, BK_FUNCTION_DISABLED,
      "Непроверенные исполнительные команды не отправляются"),
    F("service.idle", "Тест РХХ", "command", "ecu", BK_SECTION_SERVICE,
      BK_FUNCTION_DISABLED,
      "Непроверенные исполнительные команды не отправляются"),
    F("service.check_engine", "Тест лампы Check Engine", "command", "ecu",
      BK_SECTION_SERVICE, BK_FUNCTION_DISABLED,
      "Непроверенные исполнительные команды не отправляются"),
    F("service.rcod", "Коррекция RCOD", "command", "ecu", BK_SECTION_SERVICE,
      BK_FUNCTION_DISABLED,
      "Семантика и команда зависят от ПО ЭБУ; запись выключена"),
    F("service.rcok", "Коррекция RCOK", "command", "ecu", BK_SECTION_SERVICE,
      BK_FUNCTION_DISABLED,
      "Семантика и команда зависят от ПО ЭБУ; запись выключена"),
    F("service.ignition", "Коррекция УОЗ", "command", "ecu", BK_SECTION_SERVICE,
      BK_FUNCTION_DISABLED, "Нет проверенного безопасного пути записи"),
    F("service.motor_tests", "Автоматические мотор-тесты", "command",
      "ecu;software", BK_SECTION_SERVICE, BK_FUNCTION_DISABLED,
      "Нет проверенных алгоритмов, условий и безопасного завершения")};
#undef F
const bk_function_t *bk_function_catalog(size_t *count) {
  if (count)
    *count = sizeof(catalog) / sizeof(catalog[0]);
  return catalog;
}
const bk_function_t *bk_function_find(const char *id) {
  if (!id)
    return NULL;
  for (size_t i = 0; i < sizeof(catalog) / sizeof(catalog[0]); i++)
    if (!strcmp(catalog[i].id, id))
      return &catalog[i];
  return NULL;
}
const char *bk_function_status_name(bk_function_status_t s) {
  switch (s) {
  case BK_FUNCTION_AVAILABLE:
    return "Доступен";
  case BK_FUNCTION_NEEDS_CALIBRATION:
    return "Доступен после калибровки";
  case BK_FUNCTION_NEEDS_SENSOR:
    return "Требуется дополнительный датчик";
  case BK_FUNCTION_NO_SIGNAL:
    return "Нет сигнала";
  case BK_FUNCTION_NEEDS_PROTOCOL:
    return "Требуется подтверждение протокола";
  case BK_FUNCTION_PLANNED:
    return "Запланирован";
  case BK_FUNCTION_UNSUPPORTED:
    return "Не поддерживается";
  case BK_FUNCTION_DISABLED:
    return "Отключено";
  default:
    return "Неизвестный статус";
  }
}
