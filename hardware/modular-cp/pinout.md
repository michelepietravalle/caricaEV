# Front-end CP modulare — Pinout e integrazione

Un modulo CP dedicato (IEC 61851) tipicamente espone:
- Ingressi:
  - PWM_IN: 1 kHz, duty 10–85% (duty% * 0.6 ≈ corrente A)
  - EN/INHIBIT (opzionale): abilitazione funzione
- Uscite:
  - STATE_A/B/C/D (linee logiche) o bus UART con stato e fault
  - OK_TO_CLOSE (opzionale): consenso per chiusura contattore
  - FAULT: segnalazione guasto

Connessioni suggerite all’ESP32:
- PWM_IN ← ESP32 PIN_CP_PWM (tramite isolatore digitale ISO7721)
- STATE lines → ESP32 GPIO con pull‑downs
- FAULT → ESP32 GPIO (isolato se necessario)
- OK_TO_CLOSE (se presente) → in parallelo alla logica che pilota il contattore

Alimentazione:
- 12 V per il modulo CP (seguire datasheet)
- GND CP isolato dalla rete di potenza; PE collegato al riferimento richiesto dal modulo

Firmware:
- Definisci CP_FRONTEND_MODULAR in config.h
- Il firmware usa decode via GPIO stato A/B/C/D e regola PWM 1 kHz

Vantaggi:
- Conformità più semplice, minor rischio progettuale ed EMC più prevedibile.
