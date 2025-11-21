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

#define IR_A 18
//#define IR_B 16

//SCL -> 22, SDA -> 21, 3.3v
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BUZZER 19

//trig -> 5, echo -> 17, 5v
#define SOUND_SPEED_2 0.017 //Sound speed / 2
#define TRIG_PIN 5
#define ECHO_PIN 17

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
  //pinMode(IR_B, INPUT_PULLUP);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

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
    printOLED(data);
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
    }else

    if(data == 'b'){
      buzz_state = !buzz_state;
      digitalWrite(BUZZER, buzz_state);
      Serial.println(buzz_state ? "BUZZER ON" : "BUZZER OFF");
    }else

    if(data == 'e'){
      speed = serialBT.read();
      Serial.print("speed changed to ");
      Serial.println(speed);
      setSpeed(speed);
    }else

    if(data == 'w'){
      Serial.println("forward");
      setSpeed(speed);
      setMotors(HIGH, LOW, LOW, HIGH);
    }else
    if(data == 's'){
      Serial.println("backwards");
      setSpeed(speed);
      setMotors(LOW, HIGH, HIGH, LOW);
    }else
    if(data == 'a'){
      Serial.println("left");
      setSpeed(210);
      setMotors(HIGH, LOW, HIGH, LOW);
    }else
    if(data == 'd'){
      Serial.println("right");
      setSpeed(210);
      setMotors(LOW, HIGH, LOW, HIGH);
    }else
    if(data == 'x'){
      Serial.println("stop");
      setMotors(LOW, LOW, LOW, LOW);
    }

    Serial.println(data);
  }

  if(automatic){
    //Detect foes
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);  
    float dist = pulseIn(ECHO_PIN, HIGH) * SOUND_SPEED_2;
    
    if(dist < 10){
      //Attack
      setSpeed(255);
      setMotors(HIGH, LOW, LOW, HIGH);
    }else{
      //Rotate
      setSpeed(2);
      setMotors(LOW, HIGH, LOW, HIGH);
    }
  }
  
  delay(32);
}

void setSpeed(int speed){
  ledcWrite(ENABLE_A, speed);
  ledcWrite(ENABLE_B, speed);
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
