#include "BluetoothSerial.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define MOTOR_IN1 27
#define MOTOR_IN2 26
#define MOTOR_IN3 25
#define MOTOR_IN4 33
#define ENABLE_A 14
#define ENABLE_B 32

#define IR_A 17
#define IR_B 16

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BUZZER 19

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BluetoothSerial serialBT;
int led_state = LOW;
int buzz_state = LOW;

const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int speed = 200;

String text_buffer = "";

bool automatic = false;

void setup(){
  Serial.begin(9600);

  pinMode(2, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  //Sensors
  pinMode(IR_A, INPUT_PULLUP);
  pinMode(IR_B, INPUT_PULLUP);

  //Bluetooth
  serialBT.begin("AtomV1");  

  //Motor
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);
  pinMode(ENABLE_A, OUTPUT);
  pinMode(ENABLE_B, OUTPUT);
  ledcAttachChannel(ENABLE_A, freq, resolution, pwmChannel);
  ledcAttachChannel(ENABLE_B, freq, resolution, pwmChannel);

  ledcWrite(ENABLE_A, speed);
  ledcWrite(ENABLE_B, speed);

  //OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
}

void loop(){
  if(Serial.available()){
    String data = Serial.readString();
    serialBT.print(data);
  }
  
  if(serialBT.available()){
    char data = serialBT.read();

    //Message reading
    if(data == '-'){
      for(int i = 0; i < 64; i++){
        char c = serialBT.read();
        if(c == '-') break;
        text_buffer += c;
      }

      //Message read, do something with it
      if(text_buffer == "led"){
        led_state = !led_state;
        digitalWrite(2, led_state);
      }

      Serial.println(text_buffer);
      printOLED(text_buffer);
      text_buffer = "";
    }

    //Commands
    if(data == 'p'){
      automatic = !automatic;
    }

    if(data == 'b'){
      buzz_state = !buzz_state;
      digitalWrite(BUZZER, buzz_state);
      Serial.println(buzz_state ? "BUZZER ON" : "BUZZER OFF");
    }else

    if(data == 'e'){
      int speed = serialBT.read();
      Serial.print("speed changed to ");
      Serial.println(speed);
      ledcWrite(ENABLE_A, speed);
      ledcWrite(ENABLE_B, speed);
    }else

    if(data == 'w'){
      Serial.println("forward");
      setMotors(HIGH, LOW, LOW, HIGH);
    }else
    if(data == 's'){
      Serial.println("backwards");
      setMotors(LOW, HIGH, HIGH, LOW);
    }else
    if(data == 'a'){
      Serial.println("left");
      setMotors(HIGH, LOW, LOW, LOW);
    }else
    if(data == 'd'){
      Serial.println("right");
      setMotors(LOW, LOW, LOW, HIGH);
    }else
    if(data == 'x'){
      Serial.println("stop");
      setMotors(LOW, LOW, LOW, LOW);
    }

    Serial.println(data);
  }
  
  delay(32);
}

void setMotors(bool m1, bool m2, bool m3, bool m4){
  if(automatic){
    return;
  }
  digitalWrite(MOTOR_IN1, m1);
  digitalWrite(MOTOR_IN2, m2);
  digitalWrite(MOTOR_IN3, m3);
  digitalWrite(MOTOR_IN4, m4);
}

void printOLED(String msg){
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(msg);
  display.display();
}
