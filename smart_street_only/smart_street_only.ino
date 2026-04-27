#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <GyverOLED.h>

// === RFID ===
#define SS_PIN  44
#define RST_PIN 24
MFRC522 rfid(SS_PIN, RST_PIN);

// === OLED ===
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

// === Servo ===
Servo myservo;

// === Датчики ===
#define HC_TRIG 2
#define HC_ECHO 3
#define HC_TRIG_2 4
#define HC_ECHO_2 6

// === Позиции серво ===
#define BARRIER_CLOSED 90
#define BARRIER_OPEN 180
#define HYSTERESIS 30

// === Карта ===
bool cardAuthorized = false;
unsigned long cardTime = 0;
#define CARD_TIMEOUT 5000  // 5 секунд на подъезд

// === Состояния системы (для дисплея) ===
enum DisplayState {
  INIT_SCREEN,      // 1. Инициализация
  WAIT_CARD,        // 2. Ожидание карты
  CARD_OK,          // 3. Карта валидна
  SENSOR_1_TRIGGERED, // 4. Датчик 1 сработал
  SENSOR_2_TRIGGERED  // 5. Датчик 2 сработал
};
DisplayState displayState = INIT_SCREEN;

String cardOwner = "";  // "Левон" или "Артур"

// === Чтение и проверка карты ===
String readAndCheckCard() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return "";
  
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  // 🔥 Обновлённые имена: Левон и Артур
  if (uid == "AC E6 FC 2E") return "ЛЕВОН";
  if (uid == "4A C4 89 32") return "АРТУР";
  return "UNKNOWN";
}

// === Измерение расстояния ===
int getMm(uint8_t trig, uint8_t echo, int t) {
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  uint32_t us = pulseIn(echo, HIGH, 25000);
  if (us == 0) return 3000;
  return us * (t * 6 / 10 + 330) / 2000ul;
}

// === 🖥 ФУНКЦИЯ ОБНОВЛЕНИЯ ДИСПЛЕЯ ===
void updateDisplay() {
  oled.clear();
  oled.home();
  
  switch (displayState) {
    
    case INIT_SCREEN:
      // 1) Инициализация системы Smart street
      oled.setScale(2);
      oled.print(F("ЖК ПЛАМЯ"));
      oled.setCursor(0, 3);
      oled.setScale(1);
      oled.print(F("Инициализация..."));
      break;
      
    case WAIT_CARD:
      // 2) Ожидание карточки
      oled.setScale(2);
      oled.print(F("ЖК ПЛАМЯ"));
      oled.setCursor(0, 3);
      oled.setScale(1);
      oled.print(F("Ожидание"));
      oled.setCursor(0, 5);
      oled.print(F("карточки..."));
      break;
      
    case CARD_OK:
      // 3) Карта валидна - Привет, [Имя] + подъезжайте
      oled.setScale(1);
      oled.print(F("Карта валидна!"));
      oled.setCursor(0, 2);
      oled.setScale(2);
      oled.print(F("Привет,"));
      oled.setCursor(0, 4);
      oled.print(cardOwner);  // ЛЕВОН или АРТУР
      oled.setScale(1);
      oled.setCursor(0, 6);
      oled.print(F("подъезжайте к"));
      oled.setCursor(0, 7);
      oled.print(F("шлагбауму"));
      break;
      
    case SENSOR_1_TRIGGERED:
      // 4) Датчик 1 увидел машину - Шлагбаум открыт
      oled.setScale(2);
      oled.print(F("Шлагбаум"));
      oled.setCursor(0, 3);
      oled.print(F("ОТКРЫТ"));
      oled.setScale(1);
      oled.setCursor(0, 6);
      oled.print(F("проезжайте"));
      break;
      
    case SENSOR_2_TRIGGERED:
      // 5) Датчик 2 увидел машину - Шлагбаум закрывается
      oled.setScale(2);
      oled.print(F("Шлагбаум"));
      oled.setCursor(0, 3);
      oled.print(F("закрывается"));
      oled.setScale(1);
      oled.setCursor(0, 6);
      oled.print(F("до свидания!"));
      break;
  }
  
  oled.update();  // 🔥 Обязательно для GyverOLED!
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  
  // Servo
  myservo.attach(13);
  myservo.write(BARRIER_CLOSED);
  
  // Датчики
  pinMode(HC_TRIG, OUTPUT); pinMode(HC_ECHO, INPUT);
  pinMode(HC_TRIG_2, OUTPUT); pinMode(HC_ECHO_2, INPUT);
  
  // RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // OLED
  Wire.begin();  // Для Mega: SDA=20, SCL=21
  oled.init();
  
  // 1) Показываем инициализацию
  displayState = INIT_SCREEN;
  updateDisplay();
  delay(1500);
  
  // Переходим в режим ожидания карты
  displayState = WAIT_CARD;
  updateDisplay();
  
  Serial.println(F("✅ Smart street готов"));
}

// === LOOP ===
void loop() {
  // === 1. Проверка RFID (только в состоянии WAIT_CARD или CARD_OK) ===
  if (displayState == WAIT_CARD || displayState == CARD_OK) {
    String card = readAndCheckCard();
    
    if (card == "ЛЕВОН" || card == "АРТУР") {
      cardOwner = card;
      cardAuthorized = true;
      cardTime = millis();
      
      // 3) Карта валидна
      displayState = CARD_OK;
      updateDisplay();
      Serial.println("🔑 " + cardOwner + " — доступ разрешён!");
      
    } else if (card == "UNKNOWN") {
      // Показываем отказ на 1 секунду, затем возвращаемся к ожиданию
      oled.clear();
      oled.setScale(2);
      oled.print(F("❌ Отказ"));
      oled.setCursor(0, 4);
      oled.setScale(1);
      oled.print(F("неверная карта"));
      oled.update();
      delay(1000);
      displayState = WAIT_CARD;
      updateDisplay();
      Serial.println(F("❌ Неавторизованная карта"));
    }
  }
  
  // Таймаут карты: если не подъехали за 5 сек — сброс
  if (cardAuthorized && displayState == CARD_OK && (millis() - cardTime > CARD_TIMEOUT)) {
    cardAuthorized = false;
    displayState = WAIT_CARD;
    updateDisplay();
    Serial.println(F("⏱ Время вышло"));
  }
  
  // === 2. Считывание датчиков ===
  int t = 24;
  int dist_1 = getMm(HC_TRIG, HC_ECHO, t);
  int dist_2 = getMm(HC_TRIG_2, HC_ECHO_2, t);
  int threshold1 = analogRead(A10);
  int threshold2 = analogRead(A15);
  
  // === 3. Логика шлагбаума ===
  
  // 4) Датчик 1 + авторизация = открыть шлагбаум
  if (displayState == CARD_OK && cardAuthorized && dist_1 < threshold1 - HYSTERESIS) {
    myservo.write(BARRIER_OPEN);
    displayState = SENSOR_1_TRIGGERED;
    updateDisplay();
    Serial.println(F("→ 🚗 Датчик 1: ШЛАГБАУМ ОТКРЫТ"));
    cardAuthorized = false;  // сброс для следующего проезда
  }
  
  // 5) Датчик 2 = закрыть шлагбаум
  if (displayState == SENSOR_1_TRIGGERED && dist_2 < threshold2 - HYSTERESIS) {
    myservo.write(BARRIER_CLOSED);
    displayState = SENSOR_2_TRIGGERED;
    updateDisplay();
    Serial.println(F("→ 🚗 Датчик 2: ШЛАГБАУМ ЗАКРЫВАЕТСЯ"));
    
    // Возврат в начало цикла через паузу
    delay(1500);
    displayState = WAIT_CARD;
    updateDisplay();
  }
  
  // === Отладка в Serial ===
  Serial.print("State: "); Serial.print(displayState);
  Serial.print(" | Card: "); Serial.print(cardOwner);
  Serial.print(" | D1: "); Serial.print(dist_1);
  Serial.print(" | D2: "); Serial.println(dist_2);
  
  delay(50);
}
