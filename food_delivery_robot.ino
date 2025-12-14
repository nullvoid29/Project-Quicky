#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------------------- RFID & Servo ----------------------
#define SS_PIN   5
#define RST_PIN 22
#define SERVO_PIN 13

MFRC522 mfrc522(SS_PIN, RST_PIN);

const int PWM_FREQ = 50;
const int PWM_RES  = 16;

// For continuous rotation
const int STOP_US = 1500;
const int FORWARD_US = 1300;  // spin one way
const int BACKWARD_US = 1700; // spin the other way

bool spinningForward = false;

// ---------------------- WiFi & Motor ----------------------
const char* ssid     = "DCL";
const char* password = "asad1234";

WebServer server(80);

// Motor 1
int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 14;

// Motor 2
int motor2Pin1 = 33;
int motor2Pin2 = 25;
int enable2Pin = 32;

// PWM for motors
const int motorFreq = 30000;
const int motorRes = 8;
int dutyCycle = 0;
String valueString = String(0);

// ---------------------- Functions ----------------------

// Servo movement
void moveServo(int pulseWidth) {
  int period = 1000000 / PWM_FREQ;
  uint32_t duty = (pulseWidth * ((1 << PWM_RES) - 1)) / period;
  ledcWrite(SERVO_PIN, duty);
}

// Web handlers
void handleRoot() {
  const char html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
      html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }
      .button { -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; background-color: #4CAF50; border: none; color: white; padding: 12px 28px; text-decoration: none; font-size: 26px; margin: 1px; cursor: pointer; }
      .button2 {background-color: #555555;}
    </style>
    <script>
      function moveForward() { fetch('/forward'); }
      function moveLeft() { fetch('/left'); }
      function stopRobot() { fetch('/stop'); }
      function moveRight() { fetch('/right'); }
      function moveReverse() { fetch('/reverse'); }
      function updateMotorSpeed(pos) {
        document.getElementById('motorSpeed').innerHTML = pos;
        fetch(`/speed?value=${pos}`);
      }
    </script>
  </head>
  <body>
    <h1>ESP32 Motor Control</h1>
    <p><button class="button" onclick="moveForward()">FORWARD</button></p>
    <div style="clear: both;">
      <p>
        <button class="button" onclick="moveLeft()">LEFT</button>
        <button class="button button2" onclick="stopRobot()">STOP</button>
        <button class="button" onclick="moveRight()">RIGHT</button>
      </p>
    </div>
    <p><button class="button" onclick="moveReverse()">REVERSE</button></p>
    <p>Motor Speed: <span id="motorSpeed">0</span></p>
    <input type="range" min="0" max="100" step="25" id="motorSlider" oninput="updateMotorSpeed(this.value)" value="0"/>
  </body>
  </html>)rawliteral";
  server.send(200, "text/html", html);
}

void handleForward() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
  server.send(200);
}

void handleLeft() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
  server.send(200);
}

void handleStop() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);  
  server.send(200);
}

void handleRight() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);    
  server.send(200);
}

void handleReverse() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);          
  server.send(200);
}

void handleSpeed() {
  if (server.hasArg("value")) {
    valueString = server.arg("value");
    int value = valueString.toInt();
    if (value == 0) {
      ledcWrite(enable1Pin, 0);
      ledcWrite(enable2Pin, 0);
      digitalWrite(motor1Pin1, LOW);
      digitalWrite(motor1Pin2, LOW);
      digitalWrite(motor2Pin1, LOW);
      digitalWrite(motor2Pin2, LOW);  
    } else {
      dutyCycle = map(value, 25, 100, 200, 255);
      ledcWrite(enable1Pin, dutyCycle);
      ledcWrite(enable2Pin, dutyCycle);
    }
  }
  server.send(200);
}

// ---------------------- Setup ----------------------
void setup() {
  Serial.begin(115200);

  // ---------------------- RFID & Servo ----------------------
  SPI.begin(18, 19, 23, SS_PIN);
  mfrc522.PCD_Init();
  Serial.println("Continuous servo mode: scan card to toggle spin");
  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RES);
  moveServo(STOP_US);

  // ---------------------- Motors ----------------------
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  ledcAttach(enable1Pin, motorFreq, motorRes);
  ledcAttach(enable2Pin, motorFreq, motorRes);
  ledcWrite(enable1Pin, 0);
  ledcWrite(enable2Pin, 0);

  // ---------------------- WiFi ----------------------
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected. IP: " + WiFi.localIP().toString());

  // ---------------------- Web server ----------------------
  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/left", handleLeft);
  server.on("/stop", handleStop);
  server.on("/right", handleRight);
  server.on("/reverse", handleReverse);
  server.on("/speed", handleSpeed);
  server.begin();
}

// ---------------------- Loop ----------------------
void loop() {
  // ---------------------- RFID & Servo ----------------------
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
      uid += String(mfrc522.uid.uidByte[i], HEX);
      if (i < mfrc522.uid.size - 1) uid += ":";
    }
    uid.toUpperCase();
    Serial.print("Card UID: ");
    Serial.println(uid);

    if (uid == "82:B3:6E:05") {
      spinningForward = !spinningForward;
      if (spinningForward) {
        Serial.println("Spinning forward...");
        moveServo(FORWARD_US);
        delay(1000);
        moveServo(STOP_US);
      } else {
        Serial.println("Returning to start...");
        moveServo(BACKWARD_US);
        delay(1000);
        moveServo(STOP_US);
      }
    }

    mfrc522.PICC_HaltA();
    delay(1000);
  }

  // ---------------------- Web server ----------------------
  server.handleClient();
}
