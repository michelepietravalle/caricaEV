#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

WiFiClient net;
PubSubClient mqtt(net);

// Stato EVSE
enum EvseState { S_A, S_B, S_C, S_D, S_E, S_F };
volatile EvseState evseState = S_A;
volatile float userLimitA = 16.0f; // default da rete
volatile bool chargingEnable = true;

static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float effectiveLimitA() {
  return clamp(userLimitA, 6.0f, min(SUPPLY_LIMIT_A, CABLE_LIMIT_A));
}
static float currentToDuty(float amps) {
  // IEC 61851: A ≈ 0.6 * duty% in 10..85%
  float duty = amps / 0.6f;
  return clamp(duty, 10.0f, 85.0f);
}
void applyPwmFromCurrent(float amps) {
  float dutyPct = currentToDuty(amps);
  uint32_t maxDuty = (1u << LEDC_RES_BITS) - 1;
  uint32_t dutyVal = (uint32_t)(maxDuty * (dutyPct / 100.0f));
  ledcWrite(LEDC_CHANNEL, dutyVal);
}

void publish(const char* topic, const String& payload, bool retained=false) {
  mqtt.publish(topic, payload.c_str(), retained);
}
void publishState() {
  const char* s="A";
  switch(evseState){case S_A:s="A";break;case S_B:s="B";break;case S_C:s="C";break;case S_D:s="D";break;case S_E:s="E";break;case S_F:s="F";break;}
  publish(T_STATE, s, true);
}
void setContactor(bool on) {
  digitalWrite(PIN_CONTACTOR, on ? HIGH : LOW);
}

void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  String t = topic, msg; msg.reserve(len);
  for (unsigned int i=0;i<len;i++) msg += (char)payload[i];

  if (t == T_MAXCURR_SET) {
    float a = msg.toFloat();
    if (a >= 1 && a <= 80) {
      userLimitA = a;
      float eff = effectiveLimitA();
      applyPwmFromCurrent(eff);
      publish(T_MAXCURR, String(eff,1), true);
    }
  } else if (t == T_CHARGING_SET) {
    chargingEnable = (msg == "on" || msg == "1" || msg == "true");
    if (!chargingEnable) setContactor(false);
    publish(T_CHARGING, chargingEnable ? "on" : "off", true);
  }
}

void ensureMqtt() {
  if (mqtt.connected()) return;
  while (!mqtt.connected()) {
    String cid = "evse-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqtt.connect(cid.c_str())) {
      mqtt.subscribe(T_MAXCURR_SET);
      mqtt.subscribe(T_CHARGING_SET);
      publish(T_MAXCURR, String(effectiveLimitA(),1), true);
      publish(T_CHARGING, chargingEnable ? "on" : "off", true);
      publishState();
    } else {
      delay(1500);
    }
  }
}

#if defined(CP_FRONTEND_MODULAR)
EvseState decodeStateFromInputs() {
  if (digitalRead(PIN_CP_STATE_D)==HIGH) return S_D;
  if (digitalRead(PIN_CP_STATE_C)==HIGH) return S_C;
  if (digitalRead(PIN_CP_STATE_B)==HIGH) return S_B;
  // possibili fault dal modulo
  if (digitalRead(PIN_FAULT_IN)==HIGH) return S_E;
  return S_A;
}
#elif defined(CP_FRONTEND_DISCRETE)
EvseState decodeStateFromCPAdc() {
  // Legge CP (volt) tramite ISO_SENSE scalato su ADC
  int adc = analogRead(PIN_CP_ADC);
  float vadc = (adc / 4095.0f) * CP_ADC_VREF;
  float vcp  = vadc / CP_SCALE_V_PER_V; // tensione reale sul CP (±12 V)
  // Soglie indicative; da tarare con la tua catena analogica.
  // Tipico: Stato A ~ +12V, B ~ +9V, C ~ +6V, D ~ +3V; negativo per errori.
  if (vcp > 10.0f) return S_A; // nessun veicolo
  if (vcp > 7.5f) return S_B;  // veicolo collegato
  if (vcp > 4.5f) return S_C;  // pronto a caricare
  if (vcp > 1.5f) return S_D;  // ventilazione richiesta
  if (vcp < 0.0f) return S_E;  // errore
  return S_F;
}
#endif

void setup() {
  Serial.begin(115200);

  pinMode(PIN_CONTACTOR, OUTPUT);
  setContactor(false);

  pinMode(PIN_FAULT_IN, INPUT);

  // PWM CP
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RES_BITS);
  ledcAttachPin(PIN_CP_PWM, LEDC_CHANNEL);
  applyPwmFromCurrent(effectiveLimitA());

#if defined(CP_FRONTEND_MODULAR)
  pinMode(PIN_CP_STATE_A, INPUT);
  pinMode(PIN_CP_STATE_B, INPUT);
  pinMode(PIN_CP_STATE_C, INPUT); // GPIO34: input-only, niente pull interno
  pinMode(PIN_CP_STATE_D, INPUT); // GPIO35: input-only, niente pull interno
#elif defined(CP_FRONTEND_DISCRETE)
  analogReadResolution(12);
  // eventuale calibrazione/mediana
#endif

  // Wi‑Fi + MQTT
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
}

void loop() {
  ensureMqtt();
  mqtt.loop();

#if defined(CP_FRONTEND_MODULAR)
  EvseState newState = decodeStateFromInputs();
#elif defined(CP_FRONTEND_DISCRETE)
  EvseState newState = decodeStateFromCPAdc();
#endif

  if (newState != evseState) {
    evseState = newState;
    publishState();
  }

  bool canClose = (evseState == S_C) && chargingEnable && (digitalRead(PIN_FAULT_IN)==LOW);
  setContactor(canClose);

  static uint32_t lastPub=0;
  if (millis() - lastPub > 5000) {
    lastPub = millis();
    publish(T_POWER, "0");
    publish(T_ENERGY, "0");
    publish(T_FAULT, (digitalRead(PIN_FAULT_IN)==HIGH || evseState==S_E || evseState==S_F) ? "fault" : "");
    publish(T_MAXCURR, String(effectiveLimitA(),1), true);
  }

  delay(20);
}
