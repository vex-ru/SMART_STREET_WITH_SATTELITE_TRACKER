#include <TimeLib.h>
#include <ArduinoP13.h>
#include <Servo.h>

Servo elevation;
Servo azimuth;
Servo sweepServo;  // ← Новый сервопривод на пине 8

// Current UTC [NOT LOCAL TIME]
int CurrentHour   = 15;
int CurrentMin    = 30;
int CurrentSec    = 00;
int CurrentDay    = 27;
int CurrentMonth  = 4;
int CurrentYear   = 2026;


// Set TLEs of your desired Satellite
const char *tleName = "METEOR-M 2    ";
const char *tlel1   = "1 40069U 14037A   20125.52516225 -.00000051  00000-0 -42286-5 0  9997";
const char *tlel2   = "2 40069  98.5072 164.7117 0006720  56.5344 303.6479 14.20671878301977 ";

/* Some Frequently used TLEs:
ISS (ZARYA)
1 25544U 98067A   20141.44172200  .00000634  00000-0  19412-4 0  9992
2 25544  51.6435 125.7943 0001445 337.3184 171.9893 15.49378091227714

NOAA 15                 
1 25338U 98030A   20124.94794743  .00000070  00000-0  48035-4 0  9998
2 25338  98.7208 150.2168 0011008  38.1025 322.0931 14.25962243142633

NOAA 18                 
1 28654U 05018A   20124.89114038  .00000074  00000-0  64498-4 0  9998
2 28654  99.0478 180.8944 0014042   1.3321 358.7885 14.12507537770620

NOAA 19                 
1 33591U 09005A   20125.24606893  .00000052  00000-0  53375-4 0  9997
2 33591  99.1966 129.4427 0013696 195.7711 164.3035 14.12406155579156

METEOR-M 2              
1 40069U 14037A   20125.52516225 -.00000051  00000-0 -42286-5 0  9997
2 40069  98.5072 164.7117 0006720  56.5344 303.6479 14.20671878301977 
*/


// Set your Callsign and current location details
const char  *pcMyName = "S21TO";     // Observer name
double       dMyLAT   = +44.2117;    // Latitude: N -> +, S -> - (Mineralnye Vody)
double       dMyLON   = +43.1239;    // Longitude: E -> +, W -> - (Mineralnye Vody)
double       dMyALT   = 315.0;       // Altitude ASL (m) - approx. city elevation


int rangePin = 7;   // LED for in Range Indication
int NrangePin = 6;  // LED pin for Out of range indication

int epos = 0; 
int apos = 0;

double       dSatLAT  = 0;           // Satellite latitude
double       dSatLON  = 0;           // Satellite longitude
double       dSatAZ   = 0;           // Satellite azimuth
double       dSatEL   = 0;           // Satellite elevation

char         acBuffer[20];           // Buffer for ASCII time

// ← Параметры для серво на пине 8
const int SWEEP_PIN = 8;
const unsigned long SWEEP_DURATION = 10000; // 10 секунд в одну сторону
unsigned long sweepStart = 0;
int sweepPos = 0;
bool sweepForward = true;


void setup()
{
  setTime(CurrentHour,CurrentMin,CurrentSec,CurrentDay,CurrentMonth,CurrentYear);

  elevation.attach(9);
  azimuth.attach(10);
  sweepServo.attach(SWEEP_PIN);  // ← Инициализация нового серво

  elevation.write(epos);
  azimuth.write(apos);
  sweepServo.write(0);           // ← Начальная позиция

  pinMode(NrangePin, OUTPUT);
  pinMode(rangePin, OUTPUT);

  Serial.begin(115200);
  delay(10);

  digitalWrite(NrangePin, HIGH);
  digitalWrite(rangePin, HIGH);
  delay(5000);
  
  sweepStart = millis();  // ← Запуск таймера свипа
}


void loop()
{
  char buf[80]; 
  int i;
  int          iYear    = year();
  int          iMonth   = month();
  int          iDay     = day();
  int          iHour    = hour();
  int          iMinute  = minute();
  int          iSecond  = second();

  P13Sun Sun;
  P13DateTime MyTime(iYear, iMonth, iDay, iHour, iMinute, iSecond);
  P13Observer MyQTH(pcMyName, dMyLAT, dMyLON, dMyALT);
  P13Satellite MySAT(tleName, tlel1, tlel2);

  MyTime.ascii(acBuffer);
  MySAT.predict(MyTime);
  MySAT.latlon(dSatLAT, dSatLON);
  MySAT.elaz(MyQTH, dSatEL, dSatAZ);

  Serial.print("Azimuth: ");
  Serial.println(dSatAZ,2);
  Serial.print("Elevation: ");
  Serial.println(dSatEL,2);
  Serial.println("");

  // ← Управление серво на пине 8 (неблокирующее)
  unsigned long now = millis();
  unsigned long elapsed = now - sweepStart;
  
  if (elapsed >= SWEEP_DURATION) {
    // Конец одного направления — меняем направление
    sweepForward = !sweepForward;
    sweepStart = now;
    elapsed = 0;
  }
  
  if (sweepForward) {
    sweepPos = map(elapsed, 0, SWEEP_DURATION, 0, 140);
  } else {
    sweepPos = map(elapsed, 0, SWEEP_DURATION, 140, 0);
  }
  sweepServo.write(sweepPos);


  delay(500);

  // Servo calculation for elevation/azimuth
  epos = (int)dSatEL;
  apos = (int)dSatAZ;

  if (epos < 0) {
      digitalWrite(NrangePin, HIGH);
      digitalWrite(rangePin, LOW);
  } else {
      digitalWrite(NrangePin, LOW);
      digitalWrite(rangePin, HIGH);

      if(apos < 180) {
        apos = abs(180 - (apos));
        epos = 180-epos;
      } else {
        apos = (360-apos);
        epos = epos;
      }
      azimuth.write(apos);
      delay(15);   
      elevation.write(epos);        
  }
}