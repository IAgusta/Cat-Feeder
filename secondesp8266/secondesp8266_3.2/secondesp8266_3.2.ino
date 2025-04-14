#include <SoftwareSerial.h>
#include "HX711_ADC.h"
#include <Servo.h>

const int pwmMotorA = D1;
const int dirMotorA = D3;
int motorSpeed = 255;
int stopmotor = 0;

const int button = A0;

// HX711 circuit wiring
const int DOUT = 12; //Data pin on D6
const int CLK = 13; //Clock pin on D7
const float CALIBRATION_FACTOR = -495.36; // Load cell calibration factor

struct Data {
  int weight;
  int servoRun;
};
Data Packet;

Servo myservo;
bool servoRunStatus = false;

const int redled = D2; // Red pin LED RGB
const int greenled = D4; // Green pin RGB
const int blueled = D8; // Blue pin RGB

#define BLYNK_TEMPLATE_ID "TMPL6z2N9dDO0"
#define BLYNK_TEMPLATE_NAME "Cat Feeder"
#define BLYNK_AUTH_TOKEN "4Fgpra59MH_USJMk5vYScTZj77H9w9Da"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "177013";
char pass[] = "metamorphosis";

HX711_ADC scale(DOUT, CLK); // Create HX711 object

void setup() {
  Serial.begin(115200);
  myservo.attach(D5);
  myservo.write(0);
  pinMode(pwmMotorA, OUTPUT);
  pinMode(dirMotorA, OUTPUT);
  pinMode(redled, OUTPUT);
  pinMode(greenled, OUTPUT);
  pinMode(blueled, OUTPUT);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    merah();
    delay(500);
  }
  Serial.println("WiFi Connected");
  scale.begin(); // Initialize HX711
  scale.start(2000);
  scale.setGain(128);
  scale.tare(); // Reset scale to zero
  scale.setCalFactor(CALIBRATION_FACTOR);
  hijau();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk Connected");
  Serial.println("Waiting First Esp to Login");
  delay(5000);
}

void loop() {
  Blynk.run(); // Initialize Blynk to running
  scale.update();
  float weight = scale.getData();
  if (weight<0) weight=0;
  Serial.print("Berat : ");
  Serial.println(weight);

  if (abs(weight - Packet.weight) < 0.1 && Packet.servoRun == 1) { // Allow small tolerance for weight comparison
     servoRunStatus = true; // Set servo run flag
     Packet.servoRun = 0; // Reset servo run command for next cycle
      myservo.write(0); // Stop servo (0 angle)
      Serial.println("Servo stopped");
      hijau();
      motorRun(); //motor run to make the container drop the food
      delay(200); //times for motor run foward (0.2 second)
      motorStop(); //motor stop to make the bottle stay still
      delay(5000); //give some times for food to fell down to plate
      motorReverse();
      delay(200);
      motorStop();
      Blynk.virtualWrite(V3,"Pemberian Pakan Selesai");
      Blynk.virtualWrite(V1, 0);
    }
    else if (Packet.servoRun == 1) {
      servoRunStatus = true; // Set servo run flag
      myservo.write(90); // Move servo to 45 degrees
     Serial.println("Servo running at 90 degrees");
     //led turned to yellow the status still on progress
     kuning();
     Blynk.virtualWrite(V3,"Proses Memberikan Pakan");
    } 
    else {
      servoRunStatus = false; // Reset servo run flag if not triggered
     merah(); //led red on
     myservo.write(0); 
     scale.tare();
    }

  int nilai = analogRead(button);
  if (nilai > 600 && nilai < 650) {
  Blynk.virtualWrite(V1, 1);
  Blynk.virtualWrite(V0, 50);
  Packet.servoRun = 1;
  Packet.weight = 50;
  }
   if (nilai > 650 && nilai < 700) {
  Blynk.virtualWrite(V1, 1);
  Blynk.virtualWrite(V0, 100);
  Packet.servoRun = 1;
  Packet.weight = 100;
  }
  if (nilai > 700 && nilai < 750) {
  Blynk.virtualWrite(V1, 1);
  Blynk.virtualWrite(V0, 150);
  Packet.servoRun = 1;
  Packet.weight = 150;
  }
  if (nilai > 750 && nilai < 800) {
  Blynk.virtualWrite(V1, 1);
  Blynk.virtualWrite(V0, 200);
  Packet.servoRun = 1;
  Packet.weight = 200;
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

void merah(){
  //red led on
  digitalWrite(redled, HIGH); //red led on
  digitalWrite(greenled, LOW); //green led off
  digitalWrite(blueled, LOW); //blue led off
}

void kuning(){
  digitalWrite(redled, HIGH);
  digitalWrite(greenled, HIGH); //red + green = yellow is on
  digitalWrite(blueled, LOW); //blue led off
}

void hijau(){
  digitalWrite(redled, LOW); //red led off
  digitalWrite(greenled, HIGH); //green led on
  digitalWrite(blueled, LOW); //blue led off
}

void biru(){
  digitalWrite(redled, LOW); //red led off
  digitalWrite(greenled, LOW); //green led off
  digitalWrite(blueled, HIGH); //blue led on
}

void motorRun(){
  //activing motor A
  Serial.println("Motor Go");
 	digitalWrite(pwmMotorA, motorSpeed); //running motor to ccw
 	digitalWrite(dirMotorA, LOW); //using digital, PWM is 1 or 255 or HIGH
}

void motorStop(){
  digitalWrite(pwmMotorA, stopmotor); //using digital, PWM is 0 or LOW
  Serial.println("Motor Stop");
}

void motorReverse(){
  //send motor back
  Serial.println("Motor Back");
  digitalWrite(pwmMotorA, motorSpeed); //using digital, PWM is 1 or 255 or HIGH
 	digitalWrite(dirMotorA, HIGH); //running motor to cw
}