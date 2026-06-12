#define BLYNK_TEMPLATE_ID "TMPL6ts0fiCv7"
#define BLYNK_TEMPLATE_NAME "Monitoring Kualitas dan Volume Lateks"
#define BLYNK_AUTH_TOKEN "Sh0w-5t0T6OOePrxrQMWuOmpECBu1gSs"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include "HX711.h"

// ================= WIFI =================
char ssid[] = "rid";
char pass[] = "melawan123456";

// ================= PIN =================
#define ONE_WIRE_BUS D4
#define RELAY_PIN D1
#define PH_PIN A0

#define DT D5
#define SCK D6

// ================= HX711 =================
HX711 scale;

// hasil kalibrasi loadcell
float calibration_factor = -209.0;

// ================= DS18B20 =================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ================= VARIABEL =================
float suhu = 0;
float phValue = 7;
float weight = 0;

// nilai terakhir saat trigger
float lastSuhu = 0;
float lastPH = 0;
float lastWeight = 0;

// status sistem
bool pumpRunning = false;
bool systemLocked = false;
bool freezeDisplay = false;

// timer millis
unsigned long pumpStartTime = 0;
unsigned long freezeStartTime = 0;

BlynkTimer timer;

// =====================================================
// BACA pH CEPAT
// =====================================================

float bacaPH()
{
  long total = 0;

  for(int i = 0; i < 5; i++)
  {
    total += analogRead(PH_PIN);
    delay(2);
  }

  int analogValue = total / 5;

  float voltage =
    analogValue * (3.3 / 1023.0);

  float rawPH =
    7 - ((voltage - 2.447) * 20);

  // smoothing
  phValue =
    (phValue * 0.7) +
    (rawPH * 0.3);

  // batas
  if(phValue > 14)
    phValue = 14;

  if(phValue < 0)
    phValue = 0;

  return phValue;
}

// =====================================================
// BACA BERAT
// =====================================================

float bacaBerat()
{
  float reading =
    scale.get_units(20);

  if(abs(reading) < 1)
  {
    reading = 0;
  }

  return reading;
}

// =====================================================
// FUNCTION UTAMA
// =====================================================

void kirimData()
{
  // =========================================
  // JIKA TIDAK FREEZE
  // maka sensor tetap membaca
  // =========================================

  if(!freezeDisplay)
  {
    // suhu
    sensors.requestTemperatures();
    suhu = sensors.getTempCByIndex(0);

    // pH
    phValue = bacaPH();

    // berat
    if(scale.is_ready())
    {
      weight = bacaBerat();
    }
  }

  // =========================================
  // KONDISI TRIGGER
  //
  // suhu > 32
  // pH < 6
  // =========================================

  if(
      suhu > 32.0 &&
      phValue < 6.0 &&
      !pumpRunning &&
      !systemLocked
    )
  {
    Serial.println("KONDISI BURUK");
    Serial.println("POMPA ON 2 DETIK");

    // simpan nilai terakhir
    lastSuhu = suhu;
    lastPH = phValue;
    lastWeight = weight;

    // pompa ON
    digitalWrite(RELAY_PIN, LOW);

    Blynk.virtualWrite(V2, 1);

    pumpRunning = true;

    systemLocked = true;

    pumpStartTime = millis();
  }

  // =========================================
  // POMPA MATI SETELAH 2 DETIK
  // =========================================

  if(pumpRunning)
  {
    if(millis() - pumpStartTime >= 2000)
    {
      Serial.println("POMPA OFF");

      // pompa OFF
      digitalWrite(RELAY_PIN, HIGH);

      Blynk.virtualWrite(V2, 0);

      pumpRunning = false;

      // freeze data 10 detik
      freezeDisplay = true;

      freezeStartTime = millis();
    }
  }

  // =========================================
  // FREEZE DATA 10 DETIK
  // =========================================

  if(freezeDisplay)
  {
    suhu = lastSuhu;
    phValue = lastPH;
    weight = lastWeight;

    if(millis() - freezeStartTime >= 20000)
    {
      freezeDisplay = false;

      Serial.println("SISTEM MEMBACA ULANG");
    }
  }

  // =========================================
  // RESET SYSTEM
  //
  // HARUS NORMAL DULU
  // =========================================

  if(
      suhu < 31.0 &&
      phValue >= 6.0
    )
  {
    systemLocked = false;
  }

  // =========================================
  // KIRIM KE BLYNK
  // =========================================

  Blynk.virtualWrite(V0, suhu);
  Blynk.virtualWrite(V1, phValue);
  Blynk.virtualWrite(V3, weight);

  // =========================================
  // SERIAL MONITOR
  // =========================================

  Serial.println("===== MONITORING =====");

  Serial.print("Suhu : ");
  Serial.print(suhu, 1);
  Serial.println(" C");

  Serial.print("pH : ");
  Serial.println(phValue, 2);

  Serial.print("Berat : ");
  Serial.print(weight, 1);
  Serial.println(" gram");

  if(pumpRunning)
  {
    Serial.println("Pompa : ON");
  }
  else
  {
    Serial.println("Pompa : OFF");
  }

  if(systemLocked)
  {
    Serial.println("System : LOCKED");
  }
  else
  {
    Serial.println("System : READY");
  }

  if(freezeDisplay)
  {
    Serial.println("Display : FREEZE");
  }

  Serial.println("----------------------");
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // relay
  pinMode(RELAY_PIN, OUTPUT);

  // relay OFF
  digitalWrite(RELAY_PIN, HIGH);

  // DS18B20
  sensors.begin();

  // HX711
  scale.begin(DT, SCK);

  delay(2000);

  if(scale.is_ready())
  {
    Serial.println("HX711 READY");
  }
  else
  {
    Serial.println("HX711 ERROR");
  }

  scale.set_scale(calibration_factor);

  scale.tare();

  Serial.println("TARE DONE");

  // BLYNK
  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  // baca cepat
  timer.setInterval(
    500L,
    kirimData
  );

  Serial.println("SYSTEM READY");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  Blynk.run();
  timer.run();
}