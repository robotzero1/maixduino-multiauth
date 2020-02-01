#include <Sipeed_ST7789.h>
#include "Maix_Speech_Recognition.h"
#include "voice_model.h"
#include <Wire.h>
#include "paj7620.h"

SpeechRecognizer rec;

SPIClass spi_(SPI0); // MUST be SPI0 for Maix series on board LCD
Sipeed_ST7789 lcd(320, 240, spi_);

#define PIR_MOTION_SENSOR 6
#define GES_REACTION_TIME       500
#define GES_ENTRY_TIME          800
#define GES_QUIT_TIME           1000

long security_time_out = 10000;
long test_start_millis = 0;
bool passed_auth_motion = false;
bool passed_auth_voice = false;
bool passed_auth_gesture = false;
bool passed_auth_rfid = false;
enum auth_level_states { NONE, MOTION, VOICE, GESTURE, RFID };
enum auth_level_states auth_level_passed = NONE;

int phrase_array[3];
int gesture_array[4];

unsigned char buffer[64];       // buffer array for data receive over serial port
int count = 0;
long keyfobID = 697827;         // ID printed on keyfob


void auth_motion()
{
  lcd.fillScreen(COLOR_BLACK);
  if (digitalRead(PIR_MOTION_SENSOR))
  {
    Serial.println("Hi, people is coming");
    lcd.fillRect(10, 160, 300, 70, COLOR_RED);
    lcd.setTextColor(COLOR_WHITE);
    lcd.setCursor(10, 10);
    lcd.print("Welcome To Security");
    passed_auth_motion = true;
    auth_level_passed = MOTION;
  }
}

bool auth_gesture()
{
  delay(1000);
  lcd.setCursor(10, 40);
  lcd.println("Do gesture sequence");
  long test_start_millis = millis();
  int i = 0;
  Serial.println("gesture waiting");
  delay(500);
  while (millis() - test_start_millis < security_time_out && i <= 3) {

    uint8_t data = 0;
    paj7620ReadReg(0x43, 1, &data);             // Read Bank_0_Reg_0x43/0x44 for gesture result.
    //delay(GES_QUIT_TIME);

    if (data == GES_LEFT_FLAG || data == GES_RIGHT_FLAG || data == GES_UP_FLAG || data == GES_DOWN_FLAG)
    {
      Serial.printf("Gesture : %d ", data);
      gesture_array[i] = data;
      Serial.println(gesture_array[i]);
      if (gesture_array[0] == 4 && gesture_array[1] == 8 && gesture_array[2] == 1 && gesture_array[3] == 2) {
        Serial.println("gesture accepted");
        passed_auth_gesture = true;
        auth_level_passed = GESTURE;
        memset(gesture_array, 0, sizeof(gesture_array));  // reset array
        return true;
      }
      i++;
    }
  }
  Serial.println("gesture failed");
  auth_fail();
  return false;
}

bool auth_voice()
{
  lcd.setTextColor(COLOR_WHITE);
  lcd.setCursor(10, 70);
  lcd.println("Say the pass phrase");
  rec.begin();
  rec.addVoiceModel(0, 0, red, fram_num_red);
  rec.addVoiceModel(1, 0, green, fram_num_green);
  rec.addVoiceModel(2, 0, blue, fram_num_blue);

  long test_start_millis = millis();
  int i = 0;

  while (millis() - test_start_millis < security_time_out) {
    Serial.println("voice waiting");

    int res;
    res = rec.recognize(); // need to timeout this somehow
    Serial.printf("res : %d ", res);
    if (res > 0) {
      phrase_array[i] = res;
      i++;
      if (phrase_array[0] == 1 && phrase_array[1] == 2 && phrase_array[2] == 3) {
        Serial.println("voice accepted");
        passed_auth_voice = true;
        auth_level_passed = VOICE;
        memset(phrase_array, 0, sizeof(phrase_array));  // reset array
        return true;
      }
      switch (res)
      {
        case 1:
          Serial.println("rec : red ");
          break;

        case 2:
          Serial.println("rec : green ");
          break;

        case 3:
          Serial.println("rec : blue ");
          break;
      }
    }
  }
  Serial.println("voice failed");
  auth_fail();
  return false;
}



bool auth_rfid()
{
  lcd.setCursor(10, 100);
  lcd.println("Use RIFD tag");
  long test_start_millis = millis();
  String keyfobString = "";
  String receivedString = "";
  count = 0;
  while (millis() - test_start_millis < security_time_out) {
    if (Serial2.available())
      delay(500);
    {
      while (Serial2.available())
      {
        buffer[count++] = Serial2.read();      // writing data into array
        if (count == 64)break;
      }
      for (int i = 0; i < count; i++)
      {
        if (buffer[i] > 47) { // not control char
          receivedString += (char)buffer[i];
        }
      }
      memset(buffer, 0, sizeof(buffer));  // reset buffer array
      receivedString.remove(0, 5);
      receivedString.remove(5, 2);

      keyfobString = String(keyfobID, HEX);

      if (receivedString.equalsIgnoreCase(keyfobString)) {
        Serial.println("RFID accepted");
        passed_auth_rfid = true;
        auth_level_passed = RFID;
        return true;
      }
    }

  }
  Serial.println("RFID auth failed");
  auth_fail();
  return false;
}


void auth_success()
{
  lcd.fillRect(10, 160, 300, 70, COLOR_GREEN);
  lcd.setCursor(10, 130);
  lcd.println("Door Opening...");
  auth_reset();
}

void auth_fail()
{
  lcd.fillRect(10, 160, 300, 70, COLOR_RED);
  lcd.setCursor(10, 130);
  lcd.println("Auth error. Resetting.");
  auth_reset();
}

void auth_reset()
{
  delay(3000);
  passed_auth_motion = false;
  passed_auth_voice = false;
  passed_auth_gesture = false;
  passed_auth_rfid = false;
  auth_level_passed = NONE;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, 2, 3);   //rfid reader

  pinMode(PIR_MOTION_SENSOR, INPUT);

  lcd.begin(15000000, COLOR_BLACK);
  lcd.setTextSize(2);
  uint8_t error = 0;
  paj7620Init();

}

void loop() {

  switch (auth_level_passed)
  {
    case NONE:
      auth_motion();
      break;

    case MOTION:
      auth_gesture();
      break;

    case GESTURE:
      auth_voice();
      break;

    case VOICE:
      auth_rfid();
      break;

    case RFID:
      auth_success();
      break;
  }
  
}
