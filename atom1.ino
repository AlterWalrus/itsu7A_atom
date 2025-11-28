#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define MOTOR_IN1 27
#define MOTOR_IN2 26
#define MOTOR_IN3 25
#define MOTOR_IN4 33
#define ENABLE_A 14
#define ENABLE_B 32

#define IR_PIN 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BUZZER 19

#define SOUND_SPEED_2 0.017
#define TRIG_PIN 5
#define ECHO_PIN 17

#define RF_RX 15
#define RF_TX 16

unsigned long prev_action = 0;
enum State { IDLE, ATTACKING, ROTATING, PAUSED };
State curr_state = IDLE;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
BluetoothSerial serial_bt;
WebServer server(80);

int led_state = LOW;
int buzz_state = LOW;
int speed = 200;
bool automatic = false;

char bt_buffer[128];
int bt_index = 0;
unsigned long prev_sensor = 0;
int dist = 0;
bool line = false;

const int freq = 30000;
const int pwm_channel = 0;
const int resolution = 8;

//Function Prototypes
void processCMD(char* cmd);
int getDistance();
void connectWiFi(String ssid, String pass);
void setSpeed(int s);
void setMotors(bool m1, bool m2, bool m3, bool m4);
void beep(int times);
void printOLED(String msg);
void drawHappy();
void drawSad();
void drawWow();
void drawPacMan();

void setup(){
  Serial.begin(9600);

  pinMode(2, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  //Sensors
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  //Bluetooth
  serial_bt.begin("AtomV1");

  //Wifi Server Config (Sequential)
  server.enableCORS(true); 
  server.on("/data", [](){
    char json_buffer[64];
    snprintf(json_buffer, sizeof(json_buffer), "{\"prox\":%d,\"line\":%d,\"speed\":%d,\"auto\":%d}", dist, line, speed, automatic);
    server.send(200, "application/json", json_buffer);
  });
  server.onNotFound([](){
    server.send(404, "text/plain", "Not found");
  });

  //RF
  Serial1.begin(57600, SERIAL_8N1, RF_RX, RF_TX);

  //Motors
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);
  pinMode(ENABLE_A, OUTPUT);
  pinMode(ENABLE_B, OUTPUT);
  ledcAttachChannel(ENABLE_A, freq, resolution, pwm_channel);
  ledcAttachChannel(ENABLE_B, freq, resolution, pwm_channel);

  ledcWrite(ENABLE_A, speed);
  ledcWrite(ENABLE_B, speed);

  //OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
}

void loop(){
  //Handle WiFi Client
  if (WiFi.status() == WL_CONNECTED) {
     server.handleClient(); 
  }

  //Sensors & Auto Mode
  if(millis() - prev_sensor > 100){
    dist = getDistance();
    line = !digitalRead(IR_PIN);

    if(automatic){
      autoPilot();
    }
    prev_sensor = millis();
  }

  //Bluetooth Handling
  while(serial_bt.available()){
    char c = serial_bt.read();

    if(c == '\n' || c == '\r'){
      if(bt_index > 0){
        bt_buffer[bt_index] = '\0';
        processCMD(bt_buffer);
        bt_index = 0;
      }
    }else{
      if(bt_index < 127) {
        bt_buffer[bt_index] = c;
        bt_index++;
      }
    }
  }

  //RF
  if(Serial1.available()){
    Serial.println("RF incoming!");
    String data = Serial1.readStringUntil('\n');
    if(data.startsWith("+RCV=")){
      int comma1 = data.indexOf(',');
      int comma2 = data.indexOf(',', comma1 + 1);
      int comma3 = data.indexOf(',', comma2 + 1);
      String msg = data.substring(comma2 + 1, comma3);
      Serial.println(msg);
      serial_bt.println(msg);
    }
  }
  
  //Small delay for stability
  delay(32);
}

void autoPilot(){
  unsigned long currentTime = millis();
  
  if(dist < 20){
    // Attack
    curr_state = ATTACKING;
    setSpeed(255);
    setMotors(HIGH, LOW, LOW, HIGH);
    
    // Look for line
    if(line){
      setMotors(LOW, LOW, LOW, LOW);
      curr_state = IDLE;
    }
  }else{
    // Handle rotation states
    switch(curr_state){
      case IDLE:
      case ATTACKING:
        // Start rotating
        setSpeed(180);
        setMotors(LOW, HIGH, LOW, HIGH);
        prev_action = currentTime;
        curr_state = ROTATING;
        break;
        
      case ROTATING:
        if(currentTime - prev_action >= 100){
          setMotors(LOW, LOW, LOW, LOW);
          prev_action = currentTime;
          curr_state = PAUSED;
        }
        break;
        
      case PAUSED:
        if(currentTime - prev_action >= 400){
          curr_state = IDLE;
        }
        break;
    }
  }
}

void processCMD(char* cmd) {
  String data = String(cmd);
  data.trim();
  
  Serial.print("CMD: "); Serial.println(data);

  //Emotes
  if(data == ":)") { drawHappy(); return; }
  if(data == ":(") { drawSad(); return; }
  if(data == ":o") { drawWow(); return; }
  if(data == ":v") { drawPacMan(); return; }

  //WIFI
  if (data.startsWith("WIFI:")) {
    printOLED("Conectando WiFi...");
    int comma = data.indexOf(",");
    if(comma > 0) {
      String ssid = data.substring(5, comma);
      String pass = data.substring(comma + 1);
      connectWiFi(ssid, pass);
    }
    return;
  }

  //SPEED
  if(data.startsWith("SPEED:")){
    int new_speed = atoi(cmd + 6); 
    if (new_speed >= 0 && new_speed <= 255) {
      speed = new_speed;
      setSpeed(speed);
      Serial.print("Speed set to: ");
      Serial.println(speed);
    }
    return;
  }

  //LED Toggle
  if (data == "LED") {
     led_state = !led_state;
     digitalWrite(2, led_state);
     return;
  }

  //Normal messages are sent via RF
  if(data.length() > 1){
    printOLED(data);
    sendRF(data);
    return;
  }

  //Single char commands
  char c = data.charAt(0);
  
  if (c == 'p') {
    automatic = !automatic;
    Serial.println(automatic ? "AUTO ON" : "AUTO OFF");
    setMotors(LOW, LOW, LOW, LOW);
    beep(automatic ? 2 : 1);
  }
  else if (c == 'b') {
    buzz_state = !buzz_state;
    digitalWrite(BUZZER, buzz_state);
  }
  else if (c == 'w') { setMotors(HIGH, LOW, LOW, HIGH); }
  else if (c == 's') { setMotors(LOW, HIGH, HIGH, LOW); }
  else if (c == 'a') { setMotors(HIGH, LOW, HIGH, LOW); }
  else if (c == 'd') { setMotors(LOW, HIGH, LOW, HIGH); }
  else if (c == 'x') { setMotors(LOW, LOW, LOW, LOW); }
}

int getDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);  
    float d = pulseIn(ECHO_PIN, HIGH, 30000); 
    if(d == 0) return 100; 
    return d * SOUND_SPEED_2;
}

void connectWiFi(String ssid, String pass){
  WiFi.begin(ssid.c_str(), pass.c_str());
  //Disable sleep to improve stability with BT
  WiFi.setSleep(false); 
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if(WiFi.status() == WL_CONNECTED){
      server.begin();
      String ip = WiFi.localIP().toString();
      serial_bt.println("IP: " + ip);
      printOLED(ip);
      beep(3);
  } else {
      printOLED("WiFi Error");
      beep(1);
  }
}

void setSpeed(int s){
  ledcWrite(ENABLE_A, s);
  ledcWrite(ENABLE_B, s);
}

void setMotors(bool m1, bool m2, bool m3, bool m4){
  digitalWrite(MOTOR_IN1, m1);
  digitalWrite(MOTOR_IN2, m2);
  digitalWrite(MOTOR_IN3, m3);
  digitalWrite(MOTOR_IN4, m4);
}

void beep(int times){
  for(int i = 0; i < times; i++){
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
}

void printOLED(String msg){
  display.clearDisplay();
  display.setCursor(0, 8);
  display.print(msg);
  display.display();
}

void sendRF(String data){
  int length = data.length();
  String msg = data = "AT+SEND=50," + String(length) + "," + data;
  Serial.print("Enviando: ");
  Serial.println(msg);
  Serial1.println(msg);
}

//Drawing Functions
void drawHappy(){
  Serial.println("Drawing happy face");
  display.clearDisplay();
  int y = 1;
  for(int i = 0; i < 16; i++){
    display.fillCircle(32+(i*4), 32+y, 4, WHITE);
    y += (i < 8 ? 1 : -1);
  }
  display.display();
}

void drawSad(){
  Serial.println("Drawing sad face");
  display.clearDisplay();
  int y = 1;
  for(int i = 0; i < 16; i++){
    display.fillCircle(32+(i*4), 32+y, 4, WHITE);
    y += (i < 8 ? -1 : 1);
  }
  display.display();
}

void drawWow(){
  Serial.println("Drawing wow face");
  display.clearDisplay();
  display.fillCircle(64, 32, 16, WHITE);
  display.display();
}

void drawPacMan(){
  Serial.println("Drawing pacman face");
  display.clearDisplay();
  for(int i = 0; i < 16; i++){
    display.fillCircle(32+(i*4), 32-i, 4, WHITE);
    display.fillCircle(32+(i*4), 32+i, 4, WHITE);
  }
  display.display();
}