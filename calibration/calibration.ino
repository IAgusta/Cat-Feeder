#include <HX711_ADC.h>

const int HX711_dout = 6; //data pin (Dout)
const int HX711_sck = 7; //pin CK

HX711_ADC Loadcell(HX711_dout, HX711_sck);

void setup() {
  Serial.begin(115200);
  Loadcell.begin();
  Loadcell.start(2000);
  Loadcell.tare();
  Loadcell.setCalFactor(1.0);
}

void updateWeight() {
  if (scale.update()) {
    float weight = scale.getData(); // Get the weight data
    Serial.print("Weight: ");
    Serial.print(weight);
    Serial.println(" grams");
    delay(500);  
  }
}

void loop(){
  updateWeight();
  float weight = scale.getData(); // Get the current weight value
}