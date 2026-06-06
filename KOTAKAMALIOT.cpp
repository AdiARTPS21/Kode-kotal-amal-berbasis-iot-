// ====================================================================
// [1] CORES & CREDENTIALS CONFIGURATION
// ====================================================================
#define BLYNK_TEMPLATE_ID   "TMPL6WdhdaUKP"
#define BLYNK_TEMPLATE_NAME "Kotak Amal IoT"
#define BLYNK_AUTH_TOKEN    "6d497U2I57zKk4SX-fllT1K-MPBDB3QX"

// ====================================================================
// [2] SYSTEM LIBRARIES
// ====================================================================
#include <WiFi.h>
#include <WiFiManager.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ====================================================================
// [3] HARDWARE PIN MAPPING (ESP32 30 PIN - OPTIMIZED)
// ====================================================================
#define PIN_VIBRATION     34  // Input: Sensor Getar
#define PIN_DOOR_SENSOR   13  // Input: Sensor Pintu
#define PIN_BUZZER        27  // Output: Aktuator Buzzer
#define PIN_WIFI_RESET    25  // Input: Tombol Reset WiFi

#define GPS_RX_PIN        32  // Jalur RX ESP32 -> Terhubung ke TX GPS
#define GPS_TX_PIN        33  // Jalur TX ESP32 -> Terhubung ke RX GPS

// ====================================================================
// [4] SUBSYSTEM OBJECTS INITIALIZATION
// ====================================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
WiFiManager wm;

// ====================================================================
// [5] SENSITIVITAS SENSOR GETAR
// ====================================================================

#define VIB_PULSE_THRESHOLD   1     // Butuh minimal 1 pulse untuk dianggap getaran nyata
#define VIB_WINDOW_MS         1000   // Window penghitungan pulse: 1 detik
#define VIB_DEBOUNCE_MS       80     // Debounce antar pulse: 80ms
#define VIB_BUZZER_DURATION_MS 3000  // Buzzer aktif selama 3 detik setelah deteksi

// ====================================================================
// [6] STATE MACHINE & GLOBAL VARIABLES
// ====================================================================
enum SystemState { SECURITY_OK, WARNING_VIB, WARNING_DOOR, EMERGENCY };
SystemState currentState = SECURITY_OK;

bool vibrationActive    = false;
bool doorOpen           = false;
bool doorJustClosed     = false;
bool buzzerState        = false;
bool ignoreSensors      = false;

// Variabel untuk logika sensitivitas getaran
int     vibPulseCount       = 0;
unsigned long vibWindowStart    = 0;
unsigned long lastVibPulse      = 0;
unsigned long vibBuzzerUntil    = 0;  // Kapan buzzer getar harus mati

unsigned long vibHoldUntil       = 0;
unsigned long lastDoorChange     = 0;
unsigned long doorClosedAt       = 0;
unsigned long buzzerToggleMs     = 0;
unsigned long ignoreUntil        = 0;
unsigned long lastNotifyTime     = 0;
unsigned long lastGPSTracking    = 0;
unsigned long wifiResetPressTime = 0;
unsigned long lastWifiCheck      = 0;
unsigned long lastLCDUpdate      = 0;

String lastLCDLine1 = "";
String lastLCDLine2 = "";

// ====================================================================
// [7] BLYNK INFRASTRUCTURE INTERACTION
// ====================================================================
BLYNK_CONNECTED() {
  Serial.println("\n[ SUCCESS ] Connected to Blynk Cloud Server.");
  Blynk.syncAll();
}

// Widget Button di Virtual Pin V8 untuk Reset Alarm dari HP
BLYNK_WRITE(V8) {
  if (param.asInt() == 1) {
    Serial.println("\n[ ACTION ] Remote Reset Triggered via Blynk App.");
    vibrationActive  = false;
    doorOpen         = false;
    doorJustClosed   = false;
    vibPulseCount    = 0;
    vibBuzzerUntil   = 0;
    currentState     = SECURITY_OK;
    ignoreSensors    = true;
    ignoreUntil      = millis() + 5000; // Proteksi sensor selama 5 detik

    digitalWrite(PIN_BUZZER, LOW);
    updateBlynkDashboard();
    renderLCD("SYSTEM RESET", "STATUS: SECURE");
  }
}

// ====================================================================
// [8] SYSTEM BOOTSTRAP (SETUP)
// ====================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.println("         KOTAK AMAL IoT - BLYNK ONLY FIRMWARE     ");
  Serial.println("==================================================");

  pinMode(PIN_VIBRATION, INPUT);
  pinMode(PIN_DOOR_SENSOR, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_WIFI_RESET, INPUT_PULLUP);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  renderLCD("BOOTING SYSTEM", "PLEASE WAIT...");

  Serial.println("[ INFO ] Initializing UART1 for GPS Subsystem...");
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Serial.println("[ INFO ] Checking network credentials...");
  renderLCD("NET SEARCHING...", "PORTAL ACT: 20s ");

  wm.setConfigPortalTimeout(20);
  wm.setConnectTimeout(10);
  wm.setDebugOutput(false);

  if (wm.autoConnect("SAFE-AMAL IoT")) {
    Serial.print("[ SUCCESS ] Connected to WiFi. IP: ");
    Serial.println(WiFi.localIP());
    renderLCD("WIFI CONNECTED", WiFi.localIP().toString());

    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(1000);
  } else {
    Serial.println("[ WARN ] Network timeout. Entering Offline Mode.");
    renderLCD("WIFI TIMEOUT", "OFFLINE MODE");
  }

  delay(2000);
  renderLCD("SYSTEM READY", "MONITORING ACTIVE");
  Serial.println("[ INFO ] System Monitoring Active. Standby...");
  Serial.println("==================================================\n");
}

// ====================================================================
// [9] MAIN RUNTIME LOOP
// ====================================================================
void loop() {
  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.run();
  }

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (ignoreSensors && millis() > ignoreUntil) ignoreSensors = false;

  processSensorReading();
  evaluateSystemState();
  executeBuzzerPattern();
  executeGPSTracking();
  monitorNetworkStability();
  handleHardwareResetButton();
  refreshLCDDisplay();
}

// ====================================================================
// [10] CORE LOGIC - SENSOR PROCESSING (SENSITVITAS BARU)
// ====================================================================
void processSensorReading() {
  if (ignoreSensors) return;

  bool vibRaw = (digitalRead(PIN_VIBRATION) == LOW);

  if (vibRaw && (millis() - lastVibPulse > VIB_DEBOUNCE_MS)) {
    lastVibPulse = millis();

    // Reset window jika sudah lebih dari VIB_WINDOW_MS sejak pulse pertama
    if (millis() - vibWindowStart > VIB_WINDOW_MS) {
      vibPulseCount  = 0;
      vibWindowStart = millis();
    }

    vibPulseCount++;
    Serial.printf("[ SENSOR ] Vibration pulse: %d / %d\n", vibPulseCount, VIB_PULSE_THRESHOLD);

    // Jika pulse mencapai threshold → konfirmasi getaran nyata
    if (vibPulseCount >= VIB_PULSE_THRESHOLD) {
      if (!vibrationActive) {
        vibrationActive = true;
        vibBuzzerUntil  = millis() + VIB_BUZZER_DURATION_MS;
        Serial.println("[ SENSOR ] Tamper Event CONFIRMED: Real Vibration Detected!");
      } else {
        // Perpanjang buzzer jika getaran masih berlanjut
        vibBuzzerUntil = millis() + VIB_BUZZER_DURATION_MS;
      }
      // Reset counter setelah konfirmasi agar bisa deteksi event berikutnya
      vibPulseCount  = 0;
      vibWindowStart = millis();
    }
  }

  // Matikan status vibrationActive setelah durasi buzzer habis
  if (vibrationActive && millis() > vibBuzzerUntil) {
    vibrationActive = false;
    Serial.println("[ SENSOR ] Vibration hold time expired. Back to normal.");
  }

  // --- SENSOR PINTU (HIGH = Terbuka, debounce 100ms) ---
  bool currentDoorRaw = (digitalRead(PIN_DOOR_SENSOR) == HIGH);
  static bool lastDoorRaw = false;

  if (currentDoorRaw != lastDoorRaw) {
    lastDoorChange = millis();
    lastDoorRaw    = currentDoorRaw;
  }

  if (millis() - lastDoorChange >= 100) {
    if (currentDoorRaw && !doorOpen) {
      doorOpen       = true;
      doorJustClosed = false;
      Serial.println("[ SENSOR ] Security Event: Door Opened!");
    } else if (!currentDoorRaw && doorOpen) {
      doorOpen       = false;
      doorJustClosed = true;
      doorClosedAt   = millis();
      Serial.println("[ SENSOR ] Security Event: Door Closed.");
    }
  }
}

// ====================================================================
// [11] STATE EVALUATOR & BLYNK NOTIFIER
// ====================================================================
void evaluateSystemState() {
  SystemState newState;
  if (vibrationActive && doorOpen) newState = EMERGENCY;
  else if (doorOpen)               newState = WARNING_DOOR;
  else if (vibrationActive)        newState = WARNING_VIB;
  else                             newState = SECURITY_OK;

  if (newState != currentState) {
    currentState = newState;
    String title, body;

    if (currentState == WARNING_VIB) {
      title = "*PERINGATAN: DETEKSI GETARAN*";
      body  = "Indikasi pembongkaran paksa! Kotak amal terdeteksi bergetar.";
    } else if (currentState == WARNING_DOOR) {
      title = "*PERINGATAN: PINTU DIBUKA*";
      body  = "Akses tanpa izin! Pintu kotak amal telah dibuka.";
    } else if (currentState == EMERGENCY) {
      title = "*EMERGENCY: PENCURIAN AKTIF*";
      body  = "Kotak amal digetarkan dan dibuka secara paksa secara bersamaan!";
    }

    if (currentState != SECURITY_OK) {
      String fullBody = body;
      if (gps.location.isValid()) {
        fullBody += "\n\nLokasi: https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
      } else {
        fullBody += "\n\nLokasi: Mencari sinyal GPS...";
      }
      dispatchBlynkAlert(title + "\n" + fullBody);
    }
    updateBlynkDashboard();
  }
}

// ====================================================================
// [12] BUZZER PATTERN ENGINE (DIPERBAIKI)
// ====================================================================
void executeBuzzerPattern() {
  // Hapus sisa indikasi "door just closed" setelah 2 detik
  if (doorJustClosed && !doorOpen && (millis() - doorClosedAt >= 2000)) {
    doorJustClosed = false;
  }

  if (currentState == SECURITY_OK && !doorJustClosed) {
    // Kondisi aman: pastikan buzzer mati
    // (buzzer getar dikontrol oleh vibBuzzerUntil, sudah auto-off di processSensorReading)
    if (!vibrationActive) {
      digitalWrite(PIN_BUZZER, LOW);
    }
  } else if (currentState == EMERGENCY) {
    // Darurat: buzzer nyala terus-menerus
    digitalWrite(PIN_BUZZER, HIGH);
  } else if (currentState == WARNING_VIB) {
    // Getaran terdeteksi: buzzer nyala 1x selama VIB_BUZZER_DURATION_MS (3 detik)
    // Dikontrol langsung oleh vibBuzzerUntil — cukup set HIGH selama aktif
    if (millis() < vibBuzzerUntil) {
      digitalWrite(PIN_BUZZER, HIGH);
    } else {
      digitalWrite(PIN_BUZZER, LOW);
    }
  } else if (currentState == WARNING_DOOR || doorJustClosed) {
    // Pintu dibuka: kedipan buzzer sedang (350ms interval)
    if (millis() - buzzerToggleMs >= 350) {
      buzzerToggleMs = millis();
      buzzerState    = !buzzerState;
      digitalWrite(PIN_BUZZER, buzzerState);
    }
  }
}

// ====================================================================
// [13] BLYNK ONLY ALERT DISPATCHER
// ====================================================================
void dispatchBlynkAlert(String message) {
  if (millis() - lastNotifyTime < 3000) return; // Anti-spam 3 detik

  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.logEvent("security_alert", message);
    Serial.println("[ NOTIF ] Alert pushed to Blynk App.");
  }
  lastNotifyTime = millis();
}

// ====================================================================
// [14] GPS LIVE TRACKING (BLYNK ONLY)
// ====================================================================
void executeGPSTracking() {
  if (currentState == SECURITY_OK || !gps.location.isValid() || gps.speed.kmph() < 2.0) return;

  if (millis() - lastGPSTracking >= 30000) {
    String msg = "*KOTAK AMAL SEDANG BERGERAK!*\n";
    msg += "Kecepatan: " + String(gps.speed.kmph(), 1) + " km/h\n";
    msg += "Lokasi: https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);

    dispatchBlynkAlert(msg);
    lastGPSTracking = millis();
  }
}

// ====================================================================
// [15] NETWORK MANAGEMENT
// ====================================================================
void monitorNetworkStability() {
  if (millis() - lastWifiCheck >= 15000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[ WARN ] Connection lost. Re-enabling captive portal...");
      renderLCD("WIFI DISCONNECT", "RECONNECTING...");

      wm.setConfigPortalTimeout(15);
      if (wm.startConfigPortal("SAFE-AMAL IoT")) {
        renderLCD("WIFI RECONNECTED", WiFi.localIP().toString());
        Blynk.config(BLYNK_AUTH_TOKEN);
        Blynk.connect(3000);
      }
    }
    lastWifiCheck = millis();
  }
}

void updateBlynkDashboard() {
  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    String stateLabel = "AMAN";
    if (currentState == WARNING_VIB)   stateLabel = "GETARAN MENCURIGAKAN";
    if (currentState == WARNING_DOOR)  stateLabel = "BUKA PAKSA TERDETEKSI";
    if (currentState == EMERGENCY)     stateLabel = "BAHAYA-DARURAT";

    Blynk.virtualWrite(V0, stateLabel);
    Blynk.virtualWrite(V3, vibrationActive ? 1 : 0);
    Blynk.virtualWrite(V4, doorOpen ? 1 : 0);

    if (gps.location.isValid()) {
      Blynk.virtualWrite(V1, 1, gps.location.lat(), gps.location.lng(), "Lokasi Kotak Amal");
      String mapsLink = "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
      Blynk.virtualWrite(V2, mapsLink);
      Blynk.virtualWrite(V6, String(gps.speed.kmph(), 1));
      Blynk.virtualWrite(V7, String(gps.satellites.value()));
    }
  }
}

void handleHardwareResetButton() {
  if (digitalRead(PIN_WIFI_RESET) == LOW) {
    if (wifiResetPressTime == 0) wifiResetPressTime = millis();
    if (millis() - wifiResetPressTime >= 5000) {
      renderLCD("CLEARING MEMORY", "RESTARTING ESP...");
      delay(1000);
      wm.resetSettings();
      ESP.restart();
    }
  } else {
    wifiResetPressTime = 0;
  }
}

// ====================================================================
// [16] LCD INTERACTION INTERFACE
// ====================================================================
void renderLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1); lcd.print(line2.substring(0, 16));
  lastLCDLine1 = line1; lastLCDLine2 = line2;
}

void refreshLCDDisplay() {
  if (millis() - lastLCDUpdate < 500) return;
  String l1 = "", l2 = "";

  if (currentState == SECURITY_OK) {
    l1 = "  KOTAK AMAL  ";
    l2 = "STATUS: AMAN";
  } else if (currentState == WARNING_VIB) {
    l1 = "!! WARNING !!   ";
    l2 = "GETARAN KETEDEKSI";
  } else if (currentState == WARNING_DOOR) {
    l1 = "!! ALERT !!     ";
    l2 = "DI BUKA PAKSA";
  } else {
    l1 = "!! EMERGENCY !! ";
    l2 = "SYSTEM DARURAT ";
  }

  if (l1 != lastLCDLine1 || l2 != lastLCDLine2) renderLCD(l1, l2);
  lastLCDUpdate = millis();
}
