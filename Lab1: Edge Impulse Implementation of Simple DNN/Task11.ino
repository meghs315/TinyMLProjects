#include <Wire.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_HS300x.h>

//Thresholds - Humidity, Temp, Magnitude, Light
#define HUMID_JUMP_THRESHOLD   1.0
#define TEMP_RISE_THRESHOLD    0.8
#define MAG_SHIFT_THRESHOLD    15.0
#define LIGHT_CHANGE_THRESHOLD 40

#define COOLDOWN_MS 3000

//Baseline for Thresholds
float baseHumid = 0;
float baseTemp  = 0;
float baseMag   = 0;
int baseClear   = 0;

//Cooldown Logic
unsigned long lastTriggerTime = 0;

//Helpers 
float magMagnitude(float x, float y, float z) {
  return sqrt(x*x + y*y + z*z);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!APDS.begin()) {
    Serial.println("APDS9960 failed");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("IMU failed");
    while (1);
  }

  if (!HS300x.begin()) {
    Serial.println("HS3003 failed");
    while (1);
  }

  //Baseline Calibrations
  Serial.println("Calibrating baseline... keep device still");

  float hSum = 0, tSum = 0, mSum = 0;
  int cSum = 0;

  int count = 0;
  while (count < 30) {

 
    float t = HS300x.readTemperature();
    float h = HS300x.readHumidity();

    if (IMU.magneticFieldAvailable()) {
      float x, y, z;
      IMU.readMagneticField(x, y, z);
      mSum += magMagnitude(x, y, z);
    }

    if (APDS.colorAvailable()) {
      int r, g, b, c;
      APDS.readColor(r, g, b, c);
      cSum += c;
    }

    hSum += h;
    tSum += t;

    count++;
    delay(100);
  }

  baseHumid = hSum / 30.0;
  baseTemp  = tSum / 30.0;
  baseMag   = mSum / 30.0;
  baseClear = cSum / 30;

  Serial.println("Baseline ready");
}


void loop() {

  float temp = HS300x.readTemperature();
  float humidity = HS300x.readHumidity();

  // Magnetometer
  float mx = 0, my = 0, mz = 0;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
  }
  float mag = magMagnitude(mx, my, mz);

  // APDS9960
  int r = 0, g = 0, b = 0, clear = 0;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, clear);
  }


  float humid_jump = humidity - baseHumid;
  float temp_rise  = temp - baseTemp;
  float mag_shift  = mag - baseMag;
  int light_change = abs(clear - baseClear);

  //Flags for Humidity
  int humid_event = (humid_jump > HUMID_JUMP_THRESHOLD ||
                     temp_rise > TEMP_RISE_THRESHOLD);

  int mag_event = (abs(mag_shift) > MAG_SHIFT_THRESHOLD);

  int light_event = (light_change > LIGHT_CHANGE_THRESHOLD);

  //Final Labels
  String label = "BASELINE_NORMAL";

  unsigned long now = millis();

  if (now - lastTriggerTime > COOLDOWN_MS) {

    if (mag_event) {
      label = "MAGNETIC_DISTURBANCE_EVENT";
      lastTriggerTime = now;
    }
    else if (light_event) {
      label = "LIGHT_OR_COLOR_CHANGE_EVENT";
      lastTriggerTime = now;
    }
    else if (humid_event) {
      label = "BREATH_OR_WARM_AIR_EVENT";
      lastTriggerTime = now;
    }
    else {
      label = "BASELINE_NORMAL";
    }
  }


  Serial.print("rh=");
  Serial.print(humidity);
  Serial.print(", temp=");
  Serial.print(temp);
  Serial.print(", mag=");
  Serial.print(mag);

  Serial.print(" | r=");
  Serial.print(r);
  Serial.print(", g=");
  Serial.print(g);
  Serial.print(", b=");
  Serial.print(b);
  Serial.print(", clear=");
  Serial.println(clear);

  Serial.print("humid_jump=");
  Serial.print(humid_jump);
  Serial.print(", temp_rise=");
  Serial.print(temp_rise);
  Serial.print(", mag_shift=");
  Serial.print(mag_shift);
  Serial.print(", light_change=");
  Serial.println(light_change);

  Serial.print("FINAL_LABEL=");
  Serial.println(label);


  delay(1000);
}
