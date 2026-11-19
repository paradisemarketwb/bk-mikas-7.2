// GENERATED: python3 tools/generate_layout.py
window.BK_LAYOUT = {
  "version": 1,
  "start_screen": "coolant",
  "screens": [
    {
      "id": "coolant",
      "title": "ДВИГАТЕЛЬ",
      "layout": "arc3",
      "accent": "#76d7b1",
      "enabled": true,
      "main": "coolant_c",
      "left": "ecu_voltage_v",
      "right": "rpm",
      "caption": "Температура охлаждающей жидкости"
    },
    {
      "id": "tachometer",
      "title": "ТАХОМЕТР",
      "layout": "arc3",
      "accent": "#64c9f6",
      "enabled": true,
      "main": "rpm",
      "left": "ignition_deg",
      "right": "injection_ms",
      "caption": "Обороты коленвала"
    },
    {
      "id": "trip",
      "title": "ПОЕЗДКА",
      "layout": "arc3",
      "accent": "#eec078",
      "enabled": true,
      "main": "fuel_lph",
      "left": "rpm",
      "right": "ecu_voltage_v",
      "caption": "Расход в л/ч; скорость при наличии источника"
    },
    {
      "id": "sensors",
      "title": "ДАТЧИКИ",
      "layout": "arc3",
      "accent": "#ba9df5",
      "enabled": true,
      "main": "air_mass_kgh",
      "left": "throttle_pct",
      "right": "injection_ms",
      "caption": "Расход воздуха требует сверки с OpenDiag"
    },
    {
      "id": "diagnostics",
      "title": "ДИАГНОСТИКА",
      "layout": "arc3",
      "accent": "#83d6b4",
      "enabled": true,
      "main": "dtc_count",
      "left": "ecu_voltage_v",
      "right": "rpm",
      "caption": "Только просмотр ошибок"
    }
  ],
  "metrics": {
    "coolant_c": {
      "label": "Температура ОЖ",
      "unit": "°C",
      "demo": 88,
      "precision": 0,
      "warning_high": 105
    },
    "rpm": {
      "label": "Обороты",
      "unit": "об/мин",
      "demo": 2850,
      "precision": 0,
      "warning_high": 5800
    },
    "ecu_voltage_v": {
      "label": "Напряжение ЭБУ",
      "unit": "В",
      "demo": 13.9,
      "precision": 1,
      "warning_low": 12
    },
    "fuel_lph": {
      "label": "Расход топлива",
      "unit": "л/ч",
      "demo": 4.1,
      "precision": 1
    },
    "speed_kmh": {
      "label": "Скорость",
      "unit": "км/ч",
      "demo": null,
      "precision": 0,
      "requires_source": true
    },
    "injection_ms": {
      "label": "Время впрыска",
      "unit": "мс",
      "demo": 4.2,
      "precision": 1
    },
    "air_mass_kgh": {
      "label": "Расход воздуха",
      "unit": "кг/ч",
      "demo": 14.5,
      "precision": 1,
      "requires_calibration": true
    },
    "throttle_pct": {
      "label": "Дроссель",
      "unit": "%",
      "demo": 14,
      "precision": 0
    },
    "ignition_deg": {
      "label": "УОЗ",
      "unit": "°",
      "demo": 16,
      "precision": 0
    },
    "dtc_count": {
      "label": "Ошибки ЭБУ",
      "unit": "",
      "demo": 0,
      "precision": 0
    }
  }
};
