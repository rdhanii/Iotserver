#include <Arduino.h>
#include <DMDESP.h>
#include <fonts/Mono5x7.h> 

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h> 
#include <ArduinoJson.h>
#include <Ticker.h> 

// ==========================================
// 1. DATA KONEKSI & THINGSPEAK
// ==========================================
const char* ssid = "SM5_Kaisiepo";
const char* password = "bmkgupt2024!";

const unsigned long myChannelNumber = 3216296;       
const char* myReadAPIKey = "WH4GT2B1VF1LEGJL";       

// ==========================================
// 2. KONFIGURASI P10
// ==========================================
#define DISPLAYS_WIDE 1
#define DISPLAYS_HIGH 1
DMDESP Disp(DISPLAYS_WIDE, DISPLAYS_HIGH);

Ticker displayTicker; // Timer Anti-Blink

// ==========================================
// 3. VARIABEL GLOBAL
// ==========================================
String wsData = "0";
String wdData = "-";
String timeData = "--:--";

unsigned long lastDownloadTime = 0;
const long downloadInterval = 15000; 

// --- VARIABEL ANIMASI CONTINUOUS ---
unsigned long lastFrameTime = 0;    
int frameSpeed = 65;                

// Variabel untuk logika "Gerbong Kereta"
String msgA = "";    // Pesan yang sedang tampil di depan
String msgB = "";    // Pesan berikutnya yang mengintip di belakang
int posA = 32;       // Posisi X pesan A
int gap = 8;         // Jarak antar pesan (pixel)
bool nextIsSpeed = true; // Penanda giliran data berikutnya

// ==========================================
// 4. FUNGSI INTERRUPT (SCANNER)
// ==========================================
void IRAM_ATTR scanDisplay() {
  Disp.loop();
}

// ==========================================
// 5. FUNGSI LOGIKA TAMPILAN
// ==========================================

int getCenterX(String text) {
  int textWidth = Disp.textWidth(text.c_str());
  int x = (32 - textWidth) / 2;
  if (x < 0) x = 0;
  return x;
}

void drawStaticTop() {
  int x = getCenterX(timeData);
  Disp.drawText(x, 0, timeData.c_str());
}

// Fungsi helper untuk memformat data terbaru
String getFormattedData(bool isSpeed) {
  if (isSpeed) {
    return wsData + "kt";
  } else {
    // Pastikan simbol derajat konsisten
    // Saya pakai 'o' kecil sesuai kode terakhir Anda agar aman
    return wdData + "o"; 
  }
}

// Inisialisasi awal animasi
void initScroll() {
  msgA = getFormattedData(true);  // Depan: Speed
  msgB = getFormattedData(false); // Belakang: Arah
  posA = 32; // Mulai dari kanan layar
  nextIsSpeed = true; // Giliran berikutnya setelah msgB adalah Speed lagi
}

void updateDisplayFrame() {
  // Cek Waktu Gerak
  if (millis() - lastFrameTime >= frameSpeed) {
    lastFrameTime = millis();
    
    // 1. Geser posisi ke kiri
    posA--; 

    // 2. Hitung lebar pesan A saat ini
    int widthA = Disp.textWidth(msgA.c_str());

    // 3. Cek apakah Pesan A sudah hilang total ke kiri?
    // (Posisi A + Lebarnya) < 0 berarti sudah lewat layar
    if ((posA + widthA) < 0) {
      // --- LOGIKA SWAP (Tukar Posisi) ---
      
      // Posisi A yang baru adalah posisi dimana B berada sekarang
      // Posisi B tadinya di: posA_lama + widthA + gap
      // Karena posA_lama + widthA = 0 (kurang lebih), maka posA baru = gap (kurang lebih)
      // Rumus presisi:
      posA = posA + widthA + gap; 

      // Pesan B sekarang naik pangkat jadi Pesan A
      msgA = msgB;

      // Kita ambil data baru untuk Pesan B
      msgB = getFormattedData(nextIsSpeed);
      
      // Toggle giliran untuk selanjutnya
      nextIsSpeed = !nextIsSpeed;
    }
    
    // --- MENGGAMBAR KE LAYAR ---
    Disp.clear(); 
    drawStaticTop(); // Gambar Jam Statis
    
    // Gambar Pesan A (Depan)
    Disp.drawText(posA, 9, msgA.c_str());

    // Gambar Pesan B (Belakang)
    // Posisi B selalu mengekor A dengan jarak 'gap'
    int posB = posA + widthA + gap;
    
    // Hanya gambar B jika sudah masuk area layar (posisi < 32)
    // agar efisien, meski library DMD biasanya handle clipping otomatis
    if (posB < 32) {
       Disp.drawText(posB, 9, msgB.c_str());
    }
  }
}

// ==========================================
// 6. DOWNLOAD DATA (HTTP)
// ==========================================
void getDataThingSpeak() {
  WiFiClient client;
  HTTPClient http;
  
  String url = "http://api.thingspeak.com/channels/" + String(myChannelNumber) + 
               "/feeds/last.json?api_key=" + String(myReadAPIKey) + 
               "&timezone=Asia/Jayapura";

  if (http.begin(client, url)) {
    int httpCode = http.GET(); 
    
    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        String tempWS = String(doc["field2"].as<float>(), 1); 
        // Ambil WD sebagai angka murni saja dulu
        int wdInt = doc["field3"].as<int>();
        String tempWD = String(wdInt); 
        String tempTime = String((const char*)doc["created_at"]);

        if(String((const char*)doc["field2"]) != "null") wsData = tempWS;
        if(String((const char*)doc["field3"]) != "null") wdData = tempWD;
        if(tempTime.length() > 15) timeData = tempTime.substring(11, 16);
        
        Serial.println("Updated: WS=" + wsData + " WD=" + wdData);
      }
    }
    http.end();
  } else {
    Serial.println("Conn Failed");
  }
}

// ==========================================
// 7. SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);

  Disp.start();
  Disp.setBrightness(80); 
  Disp.setFont(Mono5x7);  
  
  displayTicker.attach_ms(2, scanDisplay);

  Disp.clear(); 
  int xBoot = getCenterX("WIFI...");
  Disp.drawText(xBoot, 4, "WIFI...");

  WiFi.begin(ssid, password);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Disp.clear();
    int xOk = getCenterX("OK");
    Disp.drawText(xOk, 4, "OK");
    delay(1000);
    
    getDataThingSpeak(); 
    
    // Inisialisasi logika scroll
    initScroll(); 
  } else {
    Disp.clear();
    int xErr = getCenterX("ERR");
    Disp.drawText(xErr, 4, "ERR");
  }
}

void loop() {
  if (millis() - lastDownloadTime >= downloadInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      getDataThingSpeak();
    }
    lastDownloadTime = millis();
  }

  updateDisplayFrame();
}