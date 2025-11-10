// deklarasi library

#define BLYNK_TEMPLATE_ID "TMPL6ZneqsbY4"
#define BLYNK_TEMPLATE_NAME "Monitoring Kualitas Air"
#define BLYNK_AUTH_TOKEN "Yr3fy_mang9DQcxXcTWx_vrgCEKWX-Ov"

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>

#include <WiFi.h>
#include <WiFiClient.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define ENABLE_TWILIO true // Aktifkan fitur Twilio/WhatsApp (true = sertakan kode Twilio saat kompilasi)
const char* TWILIO_SID = "sasageyo"; // Account SID Twilio (ambil dari Twilio Console) — rahasiakan
const char* TWILIO_TOKEN = "lalesha123"; // Auth Token Twilio (ambil dari Twilio Console) — rahasiakan
const char* TWILIO_FROM_WA = "4567890"; // Nomor pengirim WhatsApp Twilio (sender) — format sesuai Twilio
const char* TWILIO_TO_WA = "1234567"; // Nomor tujuan WhatsApp untuk menerima alert — gunakan format internasional tanpa '+' jika diperlukan
const char* TWILIO_CONTENT_SID // SID konten/template pesan Twilio (isi jika menggunakan Messaging Service atau template)

const unsigned long TWILIO_MIN_INTERVAL_MS = 6000 // 6 detik // Jeda minimal antar pengiriman pesan Twilio dalam milidetik
unsigned long lastTwilioSendMs = 0; // Waktu (millis) terakhir kali pesan Twilio dikirim, untuk mencegah pengiriman berulang

LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C LCD 0x27

char ssid[] = "sasageyo";
char pass[] = "lalesha123";
char auth[] = BLYNK_AUTH_TOKEN;
// Inisalisasi sensor ke esp32
namespace pin {
  const byte tds_sensor = 34;
  const byte one_wire_bus = 15; 
}

// Referensi tegangan device esp32
namespace device {
  float aref = 3.3;
}

// Deklarasi sensor
namespace sensor {
  float ec = 0;
  float tds = 0;
  float waterTemp = 0;
  float ecCalibration = 1;
}

// Inisialisasi sensor suhu
OneWire oneWire(pin::one_wire_bus);
DallasTemperature dallasTemperature(&oneWire);

void setup() {
  Serial.begin(115200);
  dallasTemperature.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("KSI RC KEL 3");
  delay(1000);
  lcd.clear();
  lcd.print("Cek Air");
  Blynk.begin(auth, ssid, pass);
  delay(1000);
  lcd.clear();
}

void loop() {
  Blynk.run();
  baca_Tds();
  delay(1000);
}

void baca_Tds() {
  // Baca suhu air
  dallasTemperature.requestTemperatures();
  sensor::waterTemp = dallasTemperature.getTempCByIndex(0);

  // Ambil rata-rata 10x pembacaan ADC untuk stabilitas
  int totalAdc = 0;
  const int samples = 10;
  for (int i = 0; i < samples; i++) {
    totalAdc += analogRead(pin::tds_sensor);
    delay(10);
  }
  int adcRaw = totalAdc / samples;

  // Konversi ADC ke tegangan (ESP32 pakai 12-bit = 0-4095)
  float rawEc = adcRaw * device::aref / 4095.0;

  // Kompensasi suhu
  float temperatureCoefficient = 1.0 + 0.02 * (sensor::waterTemp - 25.0);
  sensor::ec = (rawEc / temperatureCoefficient) * sensor::ecCalibration;

  // Rumus konversi EC ke TDS (ppm)
  sensor::tds = (133.42 * pow(sensor::ec, 3)
              - 255.86 * sensor::ec * sensor::ec
              + 857.39 * sensor::ec) * 0.5;

  // Debug serial monitor
  Serial.print("ADC Raw: "); Serial.println(adcRaw);
  Serial.print("TDS: "); Serial.println(sensor::tds, 1);
  Serial.print("Temp: "); Serial.println(sensor::waterTemp, 1);

  // Tentukan status kualitas air perlu dibanyakin status nya
  String status;
  if (sensor::tds < 150) status = "Bagus";
  else if (sensor::tds < 500) status = "Layak";
  else status = "Buruk";

  // Tampilkan ke LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TDS:");
  lcd.print(sensor::tds, 1); // 1 angka di belakang koma
  lcd.print("ppm");

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(sensor::waterTemp, 1);
  lcd.print("C ");
  lcd.print(status); // Status kualitas air

  Blynk.virtualWrite(V0,(sensor::tds));
  Blynk.virtualWrite(V2,(sensor::waterTemp));

}