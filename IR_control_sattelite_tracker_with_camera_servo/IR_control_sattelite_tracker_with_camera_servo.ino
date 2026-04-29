#include <IRremote.h>
#include <Servo.h>

// 📌 Пины
const int IR_RECEIVE_PIN = 12;
const int IR_POWER_PIN   = 13;
const int SERVO_AZIMUTH  = 10;
const int SERVO_ELEV     = 9;
const int SERVO_CAMERA   = 8;  // Серво камеры

// 🎮 Настройки телескопа
const int STEP_DEG = 5;
const bool REVERSE_AZ = false;
const bool REVERSE_EL = false;

// 📷 Настройки камеры (качание)
const int CAM_MIN = 30;       // Минимальный угол (°)
const int CAM_MAX = 150;      // Максимальный угол (°)
const int CAM_STEP = 1;       // Шаг за одно обновление (меньше = плавнее)
const unsigned long CAM_INTERVAL = 60; // Интервал в мс (больше = медленнее общий цикл)

Servo azServo;
Servo elServo;
Servo camServo;

int azAngle = 90;
int elAngle = 90;

int camAngle = CAM_MIN;
int camDir = 1; // 1 = вправо, -1 = влево
unsigned long lastCamUpdate = 0;

void setup() {
  Serial.begin(115200);
  
  // Питание ИК-приёмника
  pinMode(IR_POWER_PIN, OUTPUT);
  digitalWrite(IR_POWER_PIN, HIGH);
  delay(200);
  
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  
  // Подключение серво
  azServo.attach(SERVO_AZIMUTH);
  elServo.attach(SERVO_ELEV);
  camServo.attach(SERVO_CAMERA);
  
  // Начальные положения
  azServo.write(azAngle);
  elServo.write(elAngle);
  camServo.write(camAngle);
  
  Serial.println(F("✅ Телескоп и камера готовы."));
}

void loop() {
  // 🔍 1. Обработка ИК-сигналов
  if (IrReceiver.decode()) {
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      uint8_t cmd = IrReceiver.decodedIRData.command;
      bool changed = false;

      switch (cmd) {
        case 0x40: // Вверх
          elAngle = REVERSE_EL ? max(elAngle - STEP_DEG, 0) : min(elAngle + STEP_DEG, 180);
          changed = true; break;
        case 0x41: // Вниз
          elAngle = REVERSE_EL ? min(elAngle + STEP_DEG, 180) : max(elAngle - STEP_DEG, 0);
          changed = true; break;
        case 0x06: // Вправо
          azAngle = REVERSE_AZ ? max(azAngle - STEP_DEG, 0) : min(azAngle + STEP_DEG, 180);
          changed = true; break;
        case 0x07: // Влево
          azAngle = REVERSE_AZ ? min(azAngle + STEP_DEG, 180) : max(azAngle - STEP_DEG, 0);
          changed = true; break;
      }

      if (changed) {
        azServo.write(azAngle);
        elServo.write(elAngle);
        Serial.print(F("🔭 Az: ")); Serial.print(azAngle);
        Serial.print(F("° | El: ")); Serial.print(elAngle);
        Serial.println(F("°"));
      }
    }
    IrReceiver.resume(); // Всегда вызываем в конце декодирования
  }

  // 📷 2. Фоновое качание камеры (не блокирует loop)
  unsigned long now = millis();
  if (now - lastCamUpdate >= CAM_INTERVAL) {
    lastCamUpdate = now;
    
    camAngle += camDir * CAM_STEP;
    if (camAngle >= CAM_MAX) {
      camAngle = CAM_MAX;
      camDir = -1; // Меняем направление
    } else if (camAngle <= CAM_MIN) {
      camAngle = CAM_MIN;
      camDir = 1;  // Меняем направление
    }
    camServo.write(camAngle);
  }
}