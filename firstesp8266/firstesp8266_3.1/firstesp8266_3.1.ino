#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SimpleTimer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

//rfid
#define RST_PIN D0
#define CS_PIN D8
MFRC522 rfid(CS_PIN, RST_PIN);
// Init array that will store new NUID 
byte nuidPICC[4];
bool newcard = false;

LiquidCrystal_I2C lcd(0x27, 16, 2); //lcd 16x2 with address 0x27

// HC-SR04 reading distance
const int trigger = D4; //trig pin hcsr04
const int echo = D3; //echo pin hcsr04
const int maxDistance = 15; //max distance
float duration, distance;
// Bottle dimensions (adjust based on your actual bottle)
const float bottleRadius = 4.5; // cm (diameter is 9cm, so radius is 9/2)
const float bottleHeight = 15;   // cm
// Minimum measurable distance adjustment
const int minMeasurableDistance = 2; // cm (adjust based on your sensor)

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

struct Data {
  int weight;
  int servoRun;
};
Data Packet; //instance of the data structure (send)

//data change status
bool dataChange = false;
int previousWeight;
int previousServoRun;
bool pakanHabis = false;

SimpleTimer timer(1000);


String lastUID = "";  // store the last read UID
unsigned long lastSendTime = 0;  // store the last time UID was sent

//millis 
unsigned long lastMillis = 0;// Milliseconds since last food level measurement

#define BLYNK_TEMPLATE_ID "TMPL6z2N9dDO0"
#define BLYNK_TEMPLATE_NAME "Cat Feeder"
#define BLYNK_AUTH_TOKEN "4Fgpra59MH_USJMk5vYScTZj77H9w9Da"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "177013";
char pass[] = "metamorphosis";

void setup() {
  Serial.begin(115200);
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);
  lcd.begin();
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  lcd.backlight();
  lcd.clear();
  //wifi initialization
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    lcd.setCursor(0, 0);
    lcd.print("Please Wait");
    lcd.setCursor(0, 1);
    lcd.print("Connect to WiFi");
    delay(500);
  }
  Serial.println("WiFi Connected");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk Connected");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Connected");
  lcd.setCursor(0, 1);
  lcd.print("Blynk Connected");
  delay(5000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Mengecek Jumlah");
  lcd.setCursor(0, 1);
  lcd.print("Pakan Tersedia");
  delay(5000);
  lcd.clear();
  jumlahPakan();
}

void loop() {
  Blynk.run(); //initialize Blynk to running
  if (timer.isReady()) {
    sendUID(); // call your function when the timer is ready
    timer.reset(); // reset the timer
  }
  dataChange = (Packet.servoRun != previousServoRun);
  unsigned long currentMillis = millis();//initialize millis for pakan value
  //sending pakan value to blynk every 1 minutes
  if (currentMillis - lastMillis >= 60000) {
    lastMillis = currentMillis;
    jumlahPakan(); 
  }

  if (dataChange){
    String berat = String(Packet.weight) + "g";
    lcd.setCursor(0, 0);
    lcd.print("Beri Pakan:");
    lcd.setCursor(12, 0);
    lcd.print(berat);
  } else if (!dataChange) {
    return;
  }
  if (Packet.weight >= 50){
    int i;
    lcd.setCursor(0, 0);
    lcd.print("Terlalu Banyak, Bahaya Obesitas!");
    Blynk.virtualWrite(V3, "Terlalu Banyak, Bahaya Obesitas");
    for (i = 0 ; i < 16 ; i++){
      lcd.scrollDisplayLeft();
      delay(100);
    }
  }


}

BLYNK_WRITE(V1){
  Packet.servoRun = param.asInt() > 0;
  Serial.print(F("Status Servo dari Blynk : "));
  Serial.println(Packet.servoRun);
}

BLYNK_WRITE(V0){
  Packet.weight = param.asFloat();
  Serial.print(F("Status Jumlah Makanan dari Blynk : "));
  Serial.println(Packet.weight);
}

String tableString = ""; // global string variable to store the table string

void sendUID(){
  Serial.println("sendUID() called");
  if (! rfid.PICC_IsNewCardPresent()) {
    Serial.println("No new card present");
    return;
  }

  Serial.println("New card present");
  bool cardRead = false;
  while (!cardRead) {
    cardRead = rfid.PICC_ReadCardSerial();
    Serial.print("Card read: ");
    Serial.println(cardRead);
    delay(10); // give the RFID reader some time to read the card
  }

  String currentUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    currentUID += String(rfid.uid.uidByte[i], HEX);
  }

  Serial.print("Current UID: ");
  Serial.println(currentUID);

  char dataToSend[50]; // create a char array to hold the concatenated string
  sprintf(dataToSend, "RFID_%s %s\n", currentUID.c_str()); // concatenate the strings using sprintf

  tableString += dataToSend; // append the new RFID data to the table string

  // Clear the Terminal widget
  Blynk.virtualWrite(V4, "");

  // Display the table string in the Terminal widget
  Blynk.virtualWrite(V4, tableString);

  // Clear the table string
  tableString = "";

  // Stop reading the RFID tag
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

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
  Serial.print("Jarak : ");
  Serial.println(distance);
  return constrain(distance, 0, maxDistance); // Constrain measured distance to sensor range
}

void jumlahPakan(){
  // Measure distance to water surface
  distance = measureDistance();
  // Handle distances beyond sensor range
  if (distance > bottleHeight + bottleRadius) {
    // Distance exceeds bottle height, consider container full
    distance = bottleHeight + bottleRadius;
  } else if (distance < minMeasurableDistance) {
    // Distance less than minimum, treat as minimum
    distance = minMeasurableDistance;
  }

  // Handle invalid sensor readings (excessively high values)
  if (distance >= 1000) {
    // Set error flag and distance to minimum
    Serial.println("Error: Invalid sensor reading. Setting distance to minimum.");
    distance = minMeasurableDistance;
  }
    // Set fillPercentage to 100 if distance exceeds 50
  if (distance >= 50) {
    distance = minMeasurableDistance;
  }

    // Calculate distance from sensor to water surface (considering bottle radius)
  float waterLevel = max(0.0f, distance - bottleRadius);
  // Map distance to percentage (full at bottom, empty at top)
  float fillPercentage = map(distance, minMeasurableDistance, bottleHeight, 100, 0);
  // Ensure fillPercentage is within range (0-100)
  fillPercentage = constrain(fillPercentage, 0, 100);
  int percentageDecrease = (int)fillPercentage; // Cast to integer for percentage
  String persentase = String(percentageDecrease) + "%";
  clearLCD(1);
  Serial.print("Jumlah Pakan : ");
  Serial.println(percentageDecrease);
  Serial.println("Status Pakan Terkirim");
  lcd.setCursor(0, 1);
  lcd.print("Pakan :");
  lcd.setCursor(8, 1);
  lcd.print(persentase);

  Blynk.virtualWrite(V2, percentageDecrease);
  if (percentageDecrease <= 1) {
    pakanHabis = true;
  } else {
    pakanHabis = false;
  }
    if (pakanHabis){
      //set lcd to clear and send status to blynk
      clearLCD(0);
      lcd.setCursor(0, 0);
      lcd.print("Tambahkan Pakan");
      Blynk.virtualWrite(V3, "Mohon Isi Ulang Makanan");
    }
    else {
      static bool firstClear = true; //flag firstClear
      if (firstClear){
        //reset status
        clearLCD(0);
        Blynk.virtualWrite(V3,"-");
      }
    }
}

void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void clearLCD(int line)
{               
        lcd.setCursor(0,line);
        for(int n = 0; n < 16; n++) //indicates symbols in line
        {
                lcd.print(" ");
        }
}