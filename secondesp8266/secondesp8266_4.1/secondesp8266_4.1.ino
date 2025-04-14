#include <Ticker.h>
#include "HX711_ADC.h"
#include <Servo.h>

const int pwmMotorA = D1;
const int dirMotorA = D3;
int motorSpeed = 255;
int stopmotor = 0;

const int button = A0;

// HX711 circuit wiring
const int DOUT = D6; //Data pin on D6
const int CLK = D7; //Clock pin on D7
const float CALIBRATION_FACTOR = 2392.11; // Load cell calibration factor
Ticker weightUpdateTimer;

struct Data {
  int weight;
  int servoRun;
};
Data Packet;

Servo myservo;
Servo servoD0; // new servo for D0 pin
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

unsigned long lastButtonPress = 0; // store the last button press time
const int debounceTime = 50; // 50ms debounce time

void setup() {
  Serial.begin(115200);
  myservo.attach(D5);
  servoD0.attach(D0);
  servoD0.write(0);
  myservo.write(0);
  pinMode(pwmMotorA, OUTPUT);
  pinMode(dirMotorA, OUTPUT);
  pinMode(redled, OUTPUT);
  pinMode(greenled, OUTPUT);
  pinMode(blueled, OUTPUT);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(redled, HIGH); //red led on
    delay(500);
  }
  Serial.println("WiFi Connected");
  scale.begin(); // Initialize HX711 with default gain (128)
  scale.setCalFactor(CALIBRATION_FACTOR); // Set calibration factor (replace with your own value)
  scale.start(2000); // Start HX711 with 2-second stabilization time
  scale.tare();
  hijau();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk Connected");
  Serial.println("Waiting First Esp to Login");
  delay(5000);
  weightUpdateTimer.attach(0.1, updateWeight); // Update weight every 100ms
}

void updateWeight() {
  if (scale.update()) {
    float weight = scale.getData(); // Get the weight data
    Serial.print("Weight: ");
    Serial.print(weight);
    Serial.println(" grams");  

  }
}

void loop() {
  Blynk.run(); // Initialize Blynk to running
  updateWeight();
  float weight = scale.getData(); // Get the current weight value
  if (weight<0) {
    weight=0;
  }
  int nilai = analogRead(button);
  int previousNilai = 0; // store the previous button state
  bool pakanGiven = false; // flag to track if "Memberikan pakan" has been printed
  bool buttonPressed = false; // flag to track if the button is currently pressed
  unsigned long pressTime = 0; // store the time of the previous press
  if (nilai >= 300 && nilai <= 400 && previousNilai < 300 && millis() - lastButtonPress > debounceTime) { // button was not pressed before
    if (Packet.weight > 0) {
      Packet.weight -= 10; // decrement Packet.weight by 10 if it's not already 0
      Blynk.virtualWrite(V0, Packet.weight); // update V0 on Blynk
    }
    lastButtonPress = millis();
  }

  if (nilai >= 500 && nilai <= 600 && previousNilai < 500 && millis() - lastButtonPress > debounceTime) { // button was not pressed before
    Packet.weight += 10; // increment Packet.weight by 10
    Blynk.virtualWrite(V0, Packet.weight); // update V0 on Blynk
    lastButtonPress = millis();
  }

  if (nilai >= 700 && nilai <= 780 && millis() - lastButtonPress > debounceTime) {
    Packet.weight = 0; // reset Packet.weight to 0
    Blynk.virtualWrite(V0, Packet.weight); // update V0 on Blynk
    lastButtonPress = millis();
  }

  if (nilai >= 850 && nilai <= 900 && Packet.weight > 0 && millis() - lastButtonPress > debounceTime) { // only allow this function if Packet.weight is greater than 0
    if (!buttonPressed) { // if the button is not currently pressed
      buttonPressed = true; // set the flag to true
      if (millis() - pressTime < 1000) { // if the time since the last press is less than 1 second
        Packet.servoRun = 0; // reset Packet.servoRun to 0
        pakanGiven = false; // reset the flag
        Blynk.virtualWrite(V1, 0);
      } else {
        if (!pakanGiven) { // if "Memberikan pakan" hasn't been printed before
          Packet.servoRun = 1; // set Packet.servoRun to 1
          pakanGiven = true; // set the flag to true
          Blynk.virtualWrite(V1, 1);
        }
      }
      lastButtonPress = millis();
    }
  } else {
    buttonPressed = false; // reset the flag whenthe button is released
  }

  if (abs(Packet.weight <= weight) && Packet.servoRun == 1) { // Allow small tolerance for weight comparison
      servoRunStatus = true; // Set servo run flag
      Packet.servoRun = 0; // Reset servo run command for next cycle
      myservo.write(0); // Stop servo (0 angle)
      Serial.println("Servo stopped");
      hijau();
      motorRun(); //motor run to make the container drop the food
      delay(250); //times for motor run foward (0.25 second)
      motorStop(); //motor stop to make the bottle stay still
      delay(5000); //give some times for food to fell down to plate
      motorReverse();
      delay(250);
      motorStop();
      scale.tare();
      Blynk.virtualWrite(V3,"Pemberian Pakan Selesai");
      Blynk.virtualWrite(V1, 0);
    }
else if (Packet.servoRun == 1 && Packet.weight > 0) {
  servoRunStatus = true; // Set servo run flag
  myservo.write(90); // Move servo to 90 degrees
  Serial.println("Servo running at 90 degrees");
  static unsigned long servoTimer = 0;
  static int servoState = 0; // add a state variable to keep track of the sequence

  if (millis() - servoTimer >= 5000) { // every 3 seconds
    servoTimer = millis();

    switch (servoState) {
      case 0:
        servoD0.write(90); // move servo to 90 degrees
        Serial.println("Servo D0 running at 0 degrees");
        servoState = 1;
        break;
      case 1:
        servoD0.write(0); // move servo back to 0 degrees
        Serial.println("Servo D0 running at 90 degrees");
        servoState = 0;
        break;

    }
  }
  biru(); //led turned to blue the status still on progress
  Blynk.virtualWrite(V3, "Proses Memberikan Pakan");
}
    else {
      servoRunStatus = false; // Reset servo run flag if not triggered
      merah(); //led red on
      myservo.write(0); 
      scale.tare();
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
    if (Packet.weight >= 50) {
    Blynk.virtualWrite(V3, "To Much!, Obesity Risk");
    }
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