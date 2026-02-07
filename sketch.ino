#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// --- PINS ---
#define BUTTON_PIN  12
#define PIR_PIN     14
#define BUZZER_PIN  26
#define SERVO_PIN   27

// --- WIFI & MQTT ---
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";
const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883;

// *** TOPICS ***
const char* topic_out = "alabs/doorbell/status"; 
const char* topic_in  = "alabs/doorbell/control"; 

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorLock;

bool isSilentMode = false;
bool doorLocked = true;
unsigned long motionStartTime = 0;
bool motionActive = false;
const long motionThreshold = 3000; 

int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// NEW: A Safe Buzzer function prototype
void safeRingBuzzer();

void setup() {
  Serial.begin(115200);

  // 1. Setup Servo
  doorLock.setPeriodHertz(50); 
  doorLock.attach(SERVO_PIN, 500, 2400); 

  // 2. STARTUP RESET
  Serial.println("--- SYSTEM START ---");
  doorLock.write(0);  
  doorLocked = true;

  // 3. Setup Pins & LCD
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("System Ready");
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  checkDoorbell();
  checkBurglar();
}

// --- MOVEMENT FUNCTIONS ---

void unlockDoor() {
  Serial.println("UNLOCKING...");
  lcd.clear(); lcd.print("Unlocking...");
  for (int pos = 0; pos <= 90; pos += 5) { // Faster steps
    doorLock.write(pos);
    delay(15);
  }
  doorLocked = false;
  lcd.setCursor(0, 1); lcd.print("Door Open");
  client.publish(topic_out, "DOOR_UNLOCKED");
}

void lockDoor() {
  Serial.println("LOCKING...");
  lcd.clear(); lcd.print("Locking...");
  for (int pos = 90; pos >= 0; pos -= 5) { 
    doorLock.write(pos);
    delay(15);
  }
  doorLocked = true;
  lcd.setCursor(0, 1); lcd.print("Door Closed");
  client.publish(topic_out, "DOOR_LOCKED");
}

// --- LOGIC ---

void checkDoorbell() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) lastDebounceTime = millis();
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      Serial.println("RING!");
      client.publish(topic_out, "RINGING");
      lcd.clear(); lcd.print("Ding Dong!");
      
      if (!isSilentMode) {
        safeRingBuzzer(); // USE THE SAFE FUNCTION
      }
      delay(500); 
    }
  }
  lastButtonState = reading;
}

// *** THE FIX: A Manual Beeper that doesn't break the Servo ***
void safeRingBuzzer() {
  // Beep 1
  for(int i=0; i<80; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(1000); 
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(1000);
  }
  delay(100);
  // Beep 2
  for(int i=0; i<150; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(2000); 
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(2000);
  }
}

void checkBurglar() {
  int motionState = digitalRead(PIR_PIN);
  static unsigned long lastPrintTime = 0; // To limit print speed

  if (motionState == HIGH) {
    if (!motionActive) {
      // START OF MOTION
      motionStartTime = millis();
      motionActive = true;
      Serial.println(">>> MOTION STARTED (Timer Reset)");
    } else {
      // CONTINUOUS MOTION
      unsigned long elapsed = millis() - motionStartTime;
      
      // Print status every 1 second (1000ms) so you know it's alive
      if (millis() - lastPrintTime > 1000) {
        Serial.print("Burglar timer: ");
        Serial.print(elapsed / 1000);
        Serial.println(" sec");
        lastPrintTime = millis();
      }

      // TRIGGER ALERT
      if (elapsed > motionThreshold) {
        Serial.println("!!! ALERT SENT !!!");
        client.publish(topic_out, "BURGLAR_ALERT");
        lcd.clear(); 
        lcd.print("SECURITY ALERT");
        
        // Reset so we don't send 100 alerts per second
        motionStartTime = millis(); 
        delay(2000); // Wait 2 seconds before next scan
        lcd.clear();
        lcd.print("System Ready");
      }
    }
  } else {
    // MOTION STOPPED
    if (motionActive) {
      Serial.println("<<< Motion Stopped (Timer Cancelled)");
      motionActive = false;
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i=0; i<length; i++) msg += (char)payload[i];
  msg.trim(); 
  
  Serial.print("CMD Received: "); Serial.println(msg);

  if (msg == "UNLOCK") unlockDoor();
  else if (msg == "LOCK") lockDoor();
  else if (msg == "SILENCE") { isSilentMode = true; lcd.clear(); lcd.print("Silent Mode"); }
  else if (msg == "UNSILENCE") isSilentMode = false;
  else if (msg.startsWith("MSG:")) {
    lcd.clear(); lcd.print("Msg:"); lcd.setCursor(0,1); lcd.print(msg.substring(4));
  }
}

void setup_wifi() {
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi OK");
}

void reconnect() {
  while (!client.connected()) {
    String id = "ESP32-" + String(random(0xffff), HEX);
    if (client.connect(id.c_str())) {
      client.subscribe(topic_in);
    } else {
      delay(2000);
    }
  }
}