// Deklarasi library
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
#include <FonnteDuino.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C LCD 0x27

// Deklarasi id pass wifi dan authentikasi blynk
char ssid[] = "Xiaomi 14T";
char pass[] = "WawanMKS";
char auth[] = BLYNK_AUTH_TOKEN;

// Token FonnteDuino
FonnteDuino fonnte("Yj41jj4HRmPp3ip5uedV"); 

// Deklarasi Timer Pesan
unsigned long routinePreviousMillis = 0;
unsigned long anomalyPreviousMillis = 0;

// Interval Rutin: 21 menit (1.260.000 ms)
const long routineInterval = 1260000; 
// Interval Anomali: 10 menit (600.000 ms)
const long anomalyInterval = 600000;     
// ---------------------------------

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

  // Set waktu awal pengiriman WA
  routinePreviousMillis = millis(); 
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

  // Klasifikasi Status Kualitas Air
  String status;
  if (sensor::tds <= 50) status = "Murni";
  else if (sensor::tds <= 100) status = "Sangat Bagus";
  else if (sensor::tds <= 200) status = "Bagus";
  else if (sensor::tds <= 300) status = "Ideal";
  else if (sensor::tds <= 400) status = "Cukup Ideal";
  else if (sensor::tds <= 500) status = "Kurang Ideal";
  else status = "Buruk"; // TDS > 500
  
  // --- LOGIKA UTAMA WA DENGAN DUAL TIMER ---
  unsigned long currentMillis = millis();

  if (sensor::tds > 500) { // TDS LEBIH DARI 500 (STATUS "BURUK")
    
    // Kirim pesan Anomali setiap 10 menit
    if (currentMillis - anomalyPreviousMillis >= anomalyInterval) {
        anomalyPreviousMillis = currentMillis;
        Serial.println("!!! ANOMALI TERDETEKSI: Kirim WA Anomali (10 Menit Interval) !!!");

        // Buat pesan dinamis dengan data TDS & Suhu
        String pesan = "🚨 ANOMALI (10m): Kualitas air BURUK!\n";
        pesan += "TDS: " + String(sensor::tds, 1) + " ppm\n";
        pesan += "Suhu: " + String(sensor::waterTemp, 1) + " °C\n";
        pesan += "Periksa segera!";
        
        // No Whatsapp yang akan diberi informasi
        fonnte.sendMessage("083182236201", pesan); 
        
        // Reset timer rutin agar pesan rutin tidak dikirim saat kondisi anomali
        routinePreviousMillis = currentMillis; 
    } else {
        Serial.println("Anomali aktif, menunggu interval 10 menit berikutnya...");
    }

  } else {
    // KONDISI NORMAL/LAYAK (TDS <= 500)

    // Kirim pesan rutin setiap 21 menit
    if (currentMillis - routinePreviousMillis >= routineInterval) {
      routinePreviousMillis = currentMillis;
      Serial.println("--- KONDISI NORMAL: Kirim WA Rutin (21 Menit Interval) ---");
      
      // Buat pesan rutin dengan data TDS & Suhu
      String pesan = "✅ Pembaruan Rutin: Air saat ini " + status + ".\n";
      pesan += "TDS: " + String(sensor::tds, 1) + " ppm\n";
      pesan += "Suhu: " + String(sensor::waterTemp, 1) + " °C";

      // No Whatsapp yang akan diberi informasi
      fonnte.sendMessage("083182236201", pesan);
      
      // Reset timer anomali
      anomalyPreviousMillis = currentMillis;
    }
  }

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

  // Kirim Tampilan Ke Blynk
  Blynk.virtualWrite(V0,(sensor::tds));
  Blynk.virtualWrite(V2,(sensor::waterTemp));
}