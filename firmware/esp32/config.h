#pragma once

// Seleziona l'opzione CP: MODULARE o DISCRETA (KiCad)
#define CP_FRONTEND_MODULAR     1
// #define CP_FRONTEND_DISCRETE  1

// Wi‑Fi / MQTT
static const char* WIFI_SSID = "SSID";
static const char* WIFI_PASS = "PASSWORD";
static const char* MQTT_HOST = "192.168.1.10";
static const uint16_t MQTT_PORT = 1883; // 8883 per TLS (richiede WiFiClientSecure)

// MQTT topics
static const char* T_STATE        = "evse/state";
static const char* T_MAXCURR      = "evse/max_current";
static const char* T_MAXCURR_SET  = "evse/max_current/set";
static const char* T_CHARGING     = "evse/charging";
static const char* T_CHARGING_SET = "evse/charging/set";
static const char* T_POWER        = "evse/power";   // placeholder (nessun MID)
static const char* T_ENERGY       = "evse/energy";  // placeholder
static const char* T_FAULT        = "evse/fault";

// Limiti impianto/cavo (PP=220Ω => 32A)
constexpr float SUPPLY_LIMIT_A = 32.0f;
constexpr float CABLE_LIMIT_A  = 32.0f;

// Pinout generico (adattare alla tua board)
static const int PIN_CP_PWM      = 25;  // PWM out verso CP frontend (isolato)
static const int PIN_CONTACTOR   = 26;  // Driver bobina
static const int PIN_FAULT_IN    = 27;  // Ingresso fault (opzionale, da RCD/modulo)

// Opzione A (modulare): input di stato dal modulo CP
static const int PIN_CP_STATE_A  = 32;
static const int PIN_CP_STATE_B  = 33;
static const int PIN_CP_STATE_C  = 34;
static const int PIN_CP_STATE_D  = 35;

// Opzione B (discreta): ADC sense CP
static const int PIN_CP_ADC      = 36;  // ADC1_CH0 (GPIO36)
static const float CP_ADC_VREF   = 3.3f;
// Fattore di scala dal tuo ISO_SENSE (da tarare in laboratorio)
static const float CP_SCALE_V_PER_V = 0.12f; // esempio: 12V in -> ~1.44V all'ADC => 0.12

// PWM 1 kHz
static const int LEDC_CHANNEL = 0;
static const int LEDC_FREQ    = 1000;
static const int LEDC_RES_BITS= 10;
