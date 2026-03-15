#include <SPI.h>
#include <MFRC522.h>

// ===== RC522 AYARLARI =====
#define RST_PIN 9
#define SS_PIN 10
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Yetkili kart UID (kendi kartınızın UID'si)
byte authorizedUID[] = {0xD9, 0x47, 0x18, 0x06};

// RC522 zaman kontrolü
unsigned long lastCardTime = 0;
const unsigned long cardWaitTime = 3000; // 3 saniye
bool systemActive = false; // Sistem aktif mi? (doğru kart okutuldu mu?)

// ===== PIN TANIMLARI =====
// Far Sistemi
const int farLed1 = 2;
const int farLed2 = 3;
const int ldrPin = A0;
int ldrEsik = 300;
bool farlarAcik = false;

// Alev Sensörü ve Siren
int flameSensor1Analog = A1;
int flameSensor2Analog = A2;
int sirenLed1 = 4;
int sirenLed2 = 5;
int esikDegeri = 900;
unsigned long sonAlevZamani = 0;
const int gecikme = 4500;
bool sirenAktif = false;
bool ilkAlevGoruldu = false;

// Park Sensörü
const int trigPin = 7;
const int echoPin = 8;
const int VERY_CLOSE = 10;
bool parkBuzzerCalismali = false;

// MQ-2 Gaz Sensörü
const int mq2Pin = A3;
int gazEsik = 400; // Gaz eşik değeri (kalibrasyona göre ayarla)
bool gazAlgilandi = false;

// Buzzer (ORTAK)
const int commonBuzzer = 6;

void setup() {
  Serial.begin(9600);
  
  // Pin modları
  pinMode(farLed1, OUTPUT);
  pinMode(farLed2, OUTPUT);
  pinMode(ldrPin, INPUT);
  
  pinMode(flameSensor1Analog, INPUT);
  pinMode(flameSensor2Analog, INPUT);
  pinMode(sirenLed1, OUTPUT);
  pinMode(sirenLed2, OUTPUT);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  pinMode(mq2Pin, INPUT);
  
  pinMode(commonBuzzer, OUTPUT);
  
  // LED'leri kapat
  digitalWrite(farLed1, LOW);
  digitalWrite(farLed2, LOW);
  digitalWrite(sirenLed1, LOW);
  digitalWrite(sirenLed2, LOW);
  
  // SPI ve RC522 başlat
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Başlangıç animasyonu
  bootAnimation();
  
  Serial.println("========================================");
  Serial.println("   İTFAİYE ARABASI - 5 ÜNİTE SİSTEM");
  Serial.println("========================================");
  Serial.println("🔐 Kartınızı okutarak sistemi başlatın!");
  Serial.println("========================================");
  Serial.println();
}

void loop() {
  // RC522 kart kontrolü (sürekli)
  rc522Kontrol();
  
  // Eğer sistem aktif değilse diğer üniteleri çalıştırma
  if (!systemActive) {
    return;
  }
  
  // Sistem aktifse tüm üniteleri çalıştır
  farKontrol();
  alevKontrol();
  parkKontrol();
  gazKontrol();
  
  delay(50);
}

// ===== BOOT ANİMASYONU =====
void bootAnimation() {
  // LED'ler sırayla yansın
  for(int i = 2; i <= 5; i++) {
    digitalWrite(i, HIGH);
    delay(150);
  }
  delay(200);
  for(int i = 2; i <= 5; i++) {
    digitalWrite(i, LOW);
    delay(150);
  }
  
  // Başlangıç sesi
  tone(commonBuzzer, 1000, 100);
  delay(150);
  tone(commonBuzzer, 1500, 100);
  delay(150);
}

// ===== RC522 KART OKUMA =====
void rc522Kontrol() {
  // Kart var mı?
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  // Spam önleme
  unsigned long currentTime = millis();
  if (currentTime - lastCardTime < cardWaitTime) {
    unsigned long remainingTime = (cardWaitTime - (currentTime - lastCardTime)) / 1000;
    Serial.print("⏳ Bekleyin! Kalan: ");
    Serial.print(remainingTime);
    Serial.println(" sn");
    tone(commonBuzzer, 500, 100);
    mfrc522.PICC_HaltA();
    return;
  }
  
  // UID yazdır
  Serial.print("📇 Kart UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) Serial.print(" ");
  }
  Serial.println();
  
  // Kart kontrolü
  if (checkUID()) {
    // DOĞRU KART
    if (!systemActive) {
      // Sistem kapalıyken doğru kart → Aç
      systemActive = true;
      Serial.println("✅ YETKİLİ KART! SİSTEM AKTİF EDİLDİ!");
      correctCard();
    } else {
      // Sistem açıkken doğru kart → Normal kapanış
      systemActive = false;
      Serial.println("🔒 SİSTEM KAPATILDI! (Normal)");
      systemShutdown();
    }
  } else {
    // YANLIŞ KART
    if (!systemActive) {
      // Sistem kapalıyken yanlış kart → Hata göster, açma
      Serial.println("❌ YETKİSİZ KART! Erişim reddedildi.");
      wrongCard();
    } else {
      // Sistem açıkken yanlış kart → GÜVENLİK KAPANIŞI
      systemActive = false;
      Serial.println("🚨 YETKİSİZ ERİŞİM! SİSTEM ACİL KAPANIŞ!");
      securityShutdown();
    }
  }
  
  lastCardTime = millis();
  Serial.println("────────────────────────────────────");
  Serial.println();
  
  mfrc522.PICC_HaltA();
}

bool checkUID() {
  if (mfrc522.uid.size != 4) return false;
  
  for (byte i = 0; i < 4; i++) {
    if (mfrc522.uid.uidByte[i] != authorizedUID[i]) {
      return false;
    }
  }
  return true;
}

void correctCard() {
  // ===== SİSTEM AÇILIŞ ANİMASYONU =====
  // LED ve buzzer AYNI ANDA başlar
  
  // Melodik açılış müziğini başlat (arka planda)
  tone(commonBuzzer, 523, 150);  // C (Do)
  
  // 1. Tüm LED'ler aynı anda yanıp sönsün (2 kez)
  for (int i = 0; i < 2; i++) {
    digitalWrite(farLed1, HIGH);
    digitalWrite(farLed2, HIGH);
    digitalWrite(sirenLed1, HIGH);
    digitalWrite(sirenLed2, HIGH);
    delay(200);
    
    // İkinci nota (LED yanıkken)
    if (i == 0) {
      tone(commonBuzzer, 784, 150);  // G (Sol)
    }
    
    digitalWrite(farLed1, LOW);
    digitalWrite(farLed2, LOW);
    digitalWrite(sirenLed1, LOW);
    digitalWrite(sirenLed2, LOW);
    delay(200);
  }
  
  Serial.println("🚒 Sistem hazır! Tüm üniteler aktif.");
}

void wrongCard() {
  // ===== YANLIŞ KART (Sistem Kapalıyken) =====
  // LED ve buzzer AYNI ANDA
  
  // Hata sesi başlat
  tone(commonBuzzer, 200, 500);
  
  // Kırmızı LED'ler yanıp sönsün (ses ile birlikte)
  for (int i = 0; i < 3; i++) {
    digitalWrite(sirenLed2, HIGH);
    delay(150);
    digitalWrite(sirenLed2, LOW);
    delay(150);
  }
  
  // Ses bitmesini bekle
  delay(200);
}

void systemShutdown() {
  // ===== SİSTEM KAPANIŞ ANİMASYONU (Normal Kapanış) =====
  // LED ve buzzer AYNI ANDA başlar
  
  // 1. Melodik kapanış müziğini başlat
  tone(commonBuzzer, 784, 120);  // G (Sol)
  
  // 2. Far LED'leri sağ-sol-sağ-sol (4 kez) - Buzzer ile senkron
  for (int i = 0; i < 4; i++) {
    // Sağ (Far LED 2)
    digitalWrite(farLed2, HIGH);
    delay(150);
    digitalWrite(farLed2, LOW);
    
    // Nota geçişleri
    if (i == 0) tone(commonBuzzer, 659, 120);  // E (Mi)
    if (i == 1) tone(commonBuzzer, 523, 120);  // C (Do)
    if (i == 2) tone(commonBuzzer, 392, 180);  // G alçak (Son nota uzun)
    
    // Sol (Far LED 1)
    digitalWrite(farLed1, HIGH);
    delay(150);
    digitalWrite(farLed1, LOW);
  }
  
  delay(100);
  
  // 3. Tüm çıkışları temizle
  digitalWrite(farLed1, LOW);
  digitalWrite(farLed2, LOW);
  digitalWrite(sirenLed1, LOW);
  digitalWrite(sirenLed2, LOW);
  noTone(commonBuzzer);
  
  // 4. Durumları sıfırla
  farlarAcik = false;
  sirenAktif = false;
  ilkAlevGoruldu = false;
  gazAlgilandi = false;
  parkBuzzerCalismali = false;
  
  Serial.println("🔒 Sistem kapatıldı. Tüm üniteler devre dışı.");
}

void securityShutdown() {
  // ===== GÜVENLİK KAPANIŞI (Yanlış Kart - Sistem Açıkken) =====
  // TÜM LED ve BUZZER AYNI ANDA, TAM SENKRON
  
  // FAZ 1: Hızlı alarm (yüksek-düşük, 3 kez) + TÜM LED'LER
  for (int i = 0; i < 3; i++) {
    // Yüksek ton + LED'ler YAN
    tone(commonBuzzer, 1000);
    digitalWrite(farLed1, HIGH);
    digitalWrite(farLed2, HIGH);
    digitalWrite(sirenLed1, HIGH);
    digitalWrite(sirenLed2, HIGH);
    delay(120);
    
    // Düşük ton + LED'ler SÖN
    tone(commonBuzzer, 500);
    digitalWrite(farLed1, LOW);
    digitalWrite(farLed2, LOW);
    digitalWrite(sirenLed1, LOW);
    digitalWrite(sirenLed2, LOW);
    delay(120);
  }
  
  // FAZ 2: Hızlı yanıp sönme (2 kez daha)
  for (int i = 0; i < 2; i++) {
    tone(commonBuzzer, 1000);
    digitalWrite(farLed1, HIGH);
    digitalWrite(farLed2, HIGH);
    digitalWrite(sirenLed1, HIGH);
    digitalWrite(sirenLed2, HIGH);
    delay(100);
    
    noTone(commonBuzzer);
    digitalWrite(farLed1, LOW);
    digitalWrite(farLed2, LOW);
    digitalWrite(sirenLed1, LOW);
    digitalWrite(sirenLed2, LOW);
    delay(100);
  }
  
  // FAZ 3: Uzun düşük ton uyarısı + TÜM LED'LER YANIK
  tone(commonBuzzer, 200);
  digitalWrite(farLed1, HIGH);
  digitalWrite(farLed2, HIGH);
  digitalWrite(sirenLed1, HIGH);
  digitalWrite(sirenLed2, HIGH);
  delay(800);
  
  // Hepsini kapat
  noTone(commonBuzzer);
  digitalWrite(farLed1, LOW);
  digitalWrite(farLed2, LOW);
  digitalWrite(sirenLed1, LOW);
  digitalWrite(sirenLed2, LOW);
  
  // Durumları sıfırla
  farlarAcik = false;
  sirenAktif = false;
  ilkAlevGoruldu = false;
  gazAlgilandi = false;
  parkBuzzerCalismali = false;
  
  Serial.println("⚠️ GÜVENLİK NEDENİYLE SİSTEM KAPATILDI!");
}

// ===== ALEV SENSÖRÜ =====
void alevKontrol() {
  int analogValue1 = analogRead(flameSensor1Analog);
  int analogValue2 = analogRead(flameSensor2Analog);
  unsigned long suAn = millis();

  if (analogValue1 < esikDegeri || analogValue2 < esikDegeri) {
    sonAlevZamani = suAn;
    ilkAlevGoruldu = true;
    
    if (!sirenAktif) {
      sirenAktif = true;
      Serial.println("🔥 ATEŞ ALGILANDI! Siren aktif.");
    }
  }

  if (ilkAlevGoruldu && (suAn - sonAlevZamani < gecikme)) {
    sirenAktif = true;
    sirenCycleNonBlocking();
  } 
  else {
    if (sirenAktif) {
      Serial.println("✓ Siren kapatıldı.");
      sirenAktif = false;
      digitalWrite(sirenLed1, LOW);
      digitalWrite(sirenLed2, LOW);
      noTone(commonBuzzer);
    }
  }
}

void sirenCycleNonBlocking() {
  static unsigned long sonGuncelleme = 0;
  static int frekans = 400;
  static bool yukseliyor = true;
  
  unsigned long suAn = millis();
  
  if (suAn - sonGuncelleme >= 5) {
    sonGuncelleme = suAn;
    
    if (yukseliyor) {
      frekans += 10;
      if (frekans >= 1000) yukseliyor = false;
    } else {
      frekans -= 10;
      if (frekans <= 400) yukseliyor = true;
    }
    
    tone(commonBuzzer, frekans);
    fadeLEDs(frekans - 400);
  }
}

void fadeLEDs(int step) {
  int brightness = map(step, 0, 600, 0, 255);
  brightness = abs(255 - brightness);
  
  analogWrite(sirenLed1, brightness);
  analogWrite(sirenLed2, brightness);
}

// ===== PARK SENSÖRÜ =====
void parkKontrol() {
  long distance = getDistance();
  
  if (distance > 0 && distance <= VERY_CLOSE) {
    parkBuzzerCalismali = true;
  } else {
    parkBuzzerCalismali = false;
  }
  
  // Buzzer kontrolü - Öncelik: Siren > Gaz > Park
  if (!sirenAktif && !gazAlgilandi && parkBuzzerCalismali) {
    tone(commonBuzzer, 2500);
  } else if (!sirenAktif && !gazAlgilandi) {
    noTone(commonBuzzer);
  }
}

long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);
  long distance = duration * 0.034 / 2;
  
  return distance;
}

// ===== FAR SİSTEMİ =====
void farKontrol() {
  int ldrStatus = analogRead(ldrPin);
  
  if (ldrStatus <= ldrEsik) {
    if (!farlarAcik) {
      digitalWrite(farLed1, HIGH);
      digitalWrite(farLed2, HIGH);
      farlarAcik = true;
      Serial.println("💡 FARLAR AÇILDI!");
    }
  } 
  else {
    if (farlarAcik) {
      digitalWrite(farLed1, LOW);
      digitalWrite(farLed2, LOW);
      farlarAcik = false;
      Serial.println("🌞 FARLAR KAPANDI!");
    }
  }
}

// ===== MQ-2 GAZ SENSÖRÜ =====
void gazKontrol() {
  int gazDegeri = analogRead(mq2Pin);
  
  // Gaz algılandı mı?
  if (gazDegeri > gazEsik) {
    if (!gazAlgilandi) {
      gazAlgilandi = true;
      Serial.print("💨 GAZ ALGILANDI! Değer: ");
      Serial.println(gazDegeri);
    }
    
    // Gaz uyarısı (siren aktif değilse)
    if (!sirenAktif) {
      gazUyarisi();
    }
  } else {
    if (gazAlgilandi) {
      gazAlgilandi = false;
      Serial.println("✓ Gaz seviyesi normal.");
      noTone(commonBuzzer);
    }
  }
}

void gazUyarisi() {
  // Kesikli bip sesi
  static unsigned long sonBip = 0;
  static bool bipAktif = false;
  
  unsigned long suAn = millis();
  
  if (suAn - sonBip >= 500) { // 500ms aralıkla
    sonBip = suAn;
    
    if (bipAktif) {
      tone(commonBuzzer, 1800, 200); // Yüksek ton
      bipAktif = false;
    } else {
      noTone(commonBuzzer);
      bipAktif = true;
    }
  }
}