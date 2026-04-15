#include <PDM.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

//Audio and Motion
#define AUDIO_THRESHOLD  100
#define MOTION_THRESHOLD 0.15

// Light Levels
#define LIGHT_LOW  15
#define LIGHT_HIGH 30

// Proximity Threshold
#define PROX_NEAR_ON   30
#define PROX_NEAR_OFF  10

//PDM buffer
short sampleBuffer[256];
volatile int samplesRead = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

//IMU baseline
float baseAx = 0, baseAy = 0, baseAz = 0;

// Sensor values
int clearVal = 0;
int proxVal = 0;

// Proximity Baseline
int proxBase = 0;

// Stable States
int dark = 0;
int near = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  // IMU baseline
  float sx = 0, sy = 0, sz = 0;
  int count = 0;
  while (count < 20) {
    if (IMU.accelerationAvailable()) {
      float x, y, z;
      IMU.readAcceleration(x, y, z);
      sx += x; sy += y; sz += z;
      count++;
      delay(20);
    }
  }
  baseAx = sx / 20;
  baseAy = sy / 20;
  baseAz = sz / 20;

  //Proximity Baseline
  Serial.println("Calibrating proximity... keep area CLEAR");
  int sum = 0;
  int samples = 0;

  while (samples < 20) {
    if (APDS.proximityAvailable()) {
      sum += APDS.readProximity();
      samples++;
      delay(20);
    }
  }
  proxBase = sum / 20;

  Serial.print("Proximity baseline = ");
  Serial.println(proxBase);

  // Mic
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone.");
    while (1);
  }

  Serial.println("Workspace classifier started");
}

void loop() {

  //Microphone
  int micLevel = 0;
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    micLevel = sum / samplesRead;
    samplesRead = 0;
  }

  //Light (smoothed)
  if (APDS.colorAvailable()) {
    int r, g, b, c;
    APDS.readColor(r, g, b, c);
    clearVal = (clearVal + c) / 2;
  }

  //Proximity (smoothed)
  if (APDS.proximityAvailable()) {
    int p = APDS.readProximity();
    proxVal = (proxVal + p) / 2;
  }


  int adjustedProx = proxVal - proxBase;

  //IMU motion
  float motionVal = 0;
  if (IMU.accelerationAvailable()) {
    float x, y, z;
    IMU.readAcceleration(x, y, z);
    float dx = x - baseAx;
    float dy = y - baseAy;
    float dz = z - baseAz;
    motionVal = sqrt(dx*dx + dy*dy + dz*dz);
  }

  // Light
  if (clearVal < LIGHT_LOW) dark = 1;
  else if (clearVal > LIGHT_HIGH) dark = 0;

  // Proximity (using adjusted value)
  if (adjustedProx > PROX_NEAR_ON) near = 1;
  else if (adjustedProx < PROX_NEAR_OFF) near = 0;

  // Other flags
  int sound  = (micLevel  > AUDIO_THRESHOLD)  ? 1 : 0;
  int moving = (motionVal > MOTION_THRESHOLD) ? 1 : 0;

  //Rule-based classification
  String label = "UNKNOWN";

  if (!sound && !dark && !moving && !near) {
    label = "QUIET_BRIGHT_STEADY_FAR";
  }
  else if (sound && !dark && !moving && !near) {
    label = "NOISY_BRIGHT_STEADY_FAR";
  }
  else if (!sound && dark && !moving && near) {
    label = "QUIET_DARK_STEADY_NEAR";
  }
  else if (sound && !dark && moving && near) {
    label = "NOISY_BRIGHT_MOVING_NEAR";
  }
  else {
    label = "UNCLASSIFIED";  // catches everything else
  }
  
  Serial.print("raw,mic=");
  Serial.print(micLevel);
  Serial.print(",clear=");
  Serial.print(clearVal);
  Serial.print(",prox=");
  Serial.print(proxVal);
  Serial.print(",adjProx=");
  Serial.print(adjustedProx);
  Serial.print(",motion=");
  Serial.println(motionVal, 4);

  Serial.print("flags,sound=");
  Serial.print(sound);
  Serial.print(",dark=");
  Serial.print(dark);
  Serial.print(",moving=");
  Serial.print(moving);
  Serial.print(",near=");
  Serial.println(near);

  Serial.print("state: ");
  Serial.println(label);

  delay(1000);
}