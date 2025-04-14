#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <espnow.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Arduino.h>

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

//Struct data that will send to slave esp8266
struct controlData {
  int weight;
  int servoRun;
};
controlData controlPacket; //instance of the data structure
//data change status
bool dataChange = false;
int previousWeight;
int previousServoRun;
bool pakanHabis = false;

//millis 
unsigned long lastMillis = 0;// Milliseconds since last food level measurement
unsigned long lastMillisCat = 0;//Millisecond for automatic reset status cat

#define BLYNK_TEMPLATE_ID "TMPL6z2N9dDO0"
#define BLYNK_TEMPLATE_NAME "Cat Feeder"
#define BLYNK_AUTH_TOKEN "4Fgpra59MH_USJMk5vYScTZj77H9w9Da"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "177013";
char pass[] = "metamorphosis";

String success; // Variable to store if sending data was successful
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //mac address for receiver
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status){
  Serial.print("\r\nLast Packet Send Status\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len){
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  Serial.print("Bytes received: ");
  Serial.println(len);
  incomingservoRun = controlPacket.servoRun;
}

void setup() {
  int baudrate = 115200;
  Serial.begin(baudrate);
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);
  lcd.begin();
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  lcd.backlight();
  lcd.clear();
  //wifi initialization
  WiFi.begin(ssid, pass);
  //checking wifi connection
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
  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent); // get the status of Trasnmitted packet after successfully Init
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv); // Register for a callback function that will be called when data is received

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Connected");
  lcd.setCursor(0, 1);
  lcd.print("Blynk Connected");
  delay(5000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Tekan Tombol ON");
  delay(5000);
  lcd.clear();
  jumlahPakan();
}

void loop() {
  Blynk.run(); //initialize Blynk to running
  unsigned long currentMillis = millis();//initialize millis for pakan value
  unsigned long currentMillisCat = millis();//initialize millis for Cat Status
  dataChange = (controlPacket.weight != previousWeight || controlPacket.servoRun != previousServoRun);
  previousWeight = controlPacket.weight;
  previousServoRun = controlPacket.servoRun;

  getReading();

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &controlPacket, sizeof(controlPacket));
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }

  //sending pakan value to blynk every 1 hours
  if (currentMillis - lastMillis >= 3600000) {
    lastMillis = currentMillis;
    jumlahPakan(); 
  }
  
  if(!rfid.PICC_IsNewCardPresent()){
    return;
  }

  if(!rfid.PICC_ReadCardSerial()){
    return;
  }

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }
  Serial.print(F("The NUID tag is: "));
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println();
  // Convert NUID to a string for Blynk (assuming 4 bytes)
  String nuidString = "";
  for (byte i = 0; i < 4; i++) {
   nuidString += String(rfid.uid.uidByte[i], HEX);
   nuidString += " ";
  }
  // Send NUID data to Blynk virtual pin and lcd
  Blynk.virtualWrite(V3, nuidString);
  lcd.setCursor(0, 0);
  lcd.print("Cat :");
  lcd.setCursor(5, 0);
  lcd.print(nuidString);
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

void getReading() {
  weight = controlPacket.weight;
  servoRun = controlPacket.servoRun;
}

BLYNK_WRITE(V1){
  controlPacket.servoRun = param.asInt() > 0;
  Serial.print(F("Status Servo dari Blynk : "));
  Serial.println(controlPacket.servoRun);
  dataChange = true;
}

BLYNK_WRITE(V0){
  controlPacket.weight = param.asFloat();
  Serial.print(F("Status Jumlah Makanan dari Blynk : "));
  Serial.println(controlPacket.weight);
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