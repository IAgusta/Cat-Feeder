#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>

#define RST_PIN D4
#define CS_PIN D8
MFRC522 mfrc522(CS_PIN, RST_PIN);
char newTagDetected;

#define BLYNK_TEMPLATE_ID "TMPL6z2N9dDO0"
#define BLYNK_TEMPLATE_NAME "Cat Feeder"
#define BLYNK_AUTH_TOKEN "4Fgpra59MH_USJMk5vYScTZj77H9w9Da"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "177013";
char pass[] = "metamorphosis";

WiFiClient client;

const int trigger = A0; //trig pin hcsr04
const int echo = D3; //echo pin hcsr04

//adjusting for serial connection using softwareserial.h
const int rxpin = 3;
const int txpin = 1;
SoftwareSerial esp = SoftwareSerial(rxpin, txpin);


LiquidCrystal_I2C lcd(0x27, 16, 2); //lcd 16x2 with address 0x27

const int maxDistance = 20;

float duration, distance;
float measureDistance() {
  // Trigger ultrasonic sensor
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);

  // Measure pulse duration
  duration = pulseIn(echo, HIGH);

  // Calculate distance (adjust based on your sensor's specifications)
  distance = duration * 0.034 / 2; // Speed of sound in cm/us

  return constrain(distance, 0, maxDistance); // Constrain measured distance to sensor range
}

//Struct data that will send to slave esp8266
struct controlData {
  float weight;
  bool servoRun;
};
controlData controlPacket; //instance of the data structure

unsigned long lastMillis = 0;// Milliseconds since last food level measurement
float percentageDecrease;
// Function for dedicated serial output
void sendDataToSerial(float percentageDecrease, bool servoRun, float weight) {
  Serial.print("Pakan : ");
  Serial.print(percentageDecrease, 1);
  Serial.println("%");
  Serial.print("Servo Run: ");
  Serial.println(servoRun ? "True" : "False");
  Serial.print("Weight: ");
  Serial.println(weight);
  Serial.flush(); // Force serial buffer flush
}


void setup() {
  esp.begin(9600);
  Serial.begin(115200);
  SPI.begin();
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);
  //checking LCD
  lcd.begin();
  lcd.clear();
  lcd.noCursor();
  lcd.print("Please Wait!");
  delay(1000);

  WiFi.begin(ssid, pass);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
	  lcd.clear();
	  lcd.setCursor(0,0);
	  lcd.print("Trying Connecting");
	  lcd.setCursor(0,1);
	  lcd.print("To WiFi");
	  delay(500);
  }

  lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Connection Done");
  delay(500);
  lcd.clear();
  jumlahPakan();
}

void loop() {
  if (Blynk.connected()) {
  esp.write((uint8_t*)&controlPacket, sizeof(controlPacket));
  // Check for 1 minute interval and call jumlahPakan()
  unsigned long currentMillis = millis();
  if (currentMillis - lastMillis >= 60000) {
    lastMillis = currentMillis;
    jumlahPakan();
  }
  }
  else {
  Serial.println("Blynk Disconnected!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Blynk Disconnect");
  }


  if (esp.available() > sizeof(controlData)) {
    controlData receivedData;
    esp.readBytes((uint8_t*) &receivedData, sizeof(controlData));
    int servoRunInt = receivedData.servoRun ? 1 : 0;
    // Update your control packet with received servoRun data
    controlPacket.servoRun = receivedData.servoRun;
    Serial.print("Received servoRun from Slave ESP: ");
    Serial.println(servoRunInt);

    // Send the updated control packet (including received servoRun) to Blynk
    Blynk.virtualWrite(V1, servoRunInt);
  }


  if (!newTagDetected && !mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Set flag to true after reading a tag
  newTagDetected = true;

  Serial.print("ID Kucing: ");
  String content = "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.setCursor(0, 1);
  lcd.print(content);
  // Send tag ID to Blynk (optional)
  delay(1000);
  newTagDetected = false;
  sendTagIDToBlynk(content);

  sendDataToSerial(percentageDecrease, controlPacket.servoRun, controlPacket.weight);
}

BLYNK_WRITE(V1){
  controlPacket.servoRun = param.asInt() > 0;
  Serial.print("Received servo control signal from blynk: ");
  Serial.println(controlPacket.servoRun);
  if (controlPacket.servoRun == 1){
    Blynk.virtualWrite(V3,"Memberikan Makan");
  }
}

BLYNK_WRITE(V0){
  controlPacket.weight = param.asFloat();
  Serial.print("Received weight from blynk: ");
  Serial.println(controlPacket.weight);
}


void jumlahPakan(){
  // Measure distance to water surface
  distance = measureDistance();
  // Calculate food level based on total container height
  float Level = 15 - distance;
  // Calculate percentage decrease
  float decrease = constrain(0.1, 0, Level);
  float percentageDecrease = (decrease / 15) * 100;
  
  Blynk.virtualWrite(V2, percentageDecrease);

  sendDataToSerial(percentageDecrease, controlPacket.servoRun, controlPacket.weight);
  if (percentageDecrease <= 5){
    Blynk.virtualWrite(V3,"Mohon Isi Ulang Makanan");
  }
}

// Function to send tag ID to Blynk
void sendTagIDToBlynk(String tagID) {
  Blynk.virtualWrite(V3, tagID);
}
