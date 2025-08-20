#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <math.h>

// OLED Display parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_ADS1115 ads;

// IR LED pin
const int IR_LED_PIN = 2;

// --- POLYNOMIAL REGRESSION FORMULAS ---
const float male_a = 5190.7;
const float male_b = -1872.4;
const float male_c = 267.32;

const float female_a = 1547.4;
const float female_b = -912.49;
const float female_c = 226.49;

// Baseline reading without a finger
int16_t baselineRawValue = 0;

enum Gender { NONE, MALE, FEMALE };
Gender userGender = NONE;

// Clustering and Averaging parameters
const int numReadings = 30;
float readings[numReadings];

void setup() {
  Serial.begin(115200);

  // Initialize IR LED pin and turn it on
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, HIGH);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Glucometer");
  display.display();
  delay(2000);

  // Initialize ADS1115
  ads.begin();


  // Step 1: Ask for gender input via Serial Monitor
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Select Gender");
  display.println("M or F");
  display.display();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Place NO finger");
  display.println("for baseline...");
  display.display();
  delay(3000);

  baselineRawValue = ads.readADC_SingleEnded(0);
  Serial.print("Baseline Raw Value: ");
  Serial.println(baselineRawValue);

  Serial.println("\n----------------------------");
  Serial.println("Enter 'M' for Male or 'F' for Female:");
  while (userGender == NONE) {
    if (Serial.available() > 0) {
      char genderInput = Serial.read();
      if (genderInput == 'M' || genderInput == 'm') {
        userGender = MALE;
        Serial.println("Male selected. Starting...");
      } else if (genderInput == 'F' || genderInput == 'f') {
        userGender = FEMALE;
        Serial.println("Female selected. Starting...");
      }
      // Consume any remaining characters in the buffer to avoid issues
      while (Serial.available()) {
        Serial.read();
      }
    }
    delay(50);
  }

  // Once a gender is selected, proceed with baseline measurement



  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Ready!");
  display.display();
  delay(7000);
}

void loop() {
  // Step 1: Instruct user to place finger and take 30 readings
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Place finger...");
  display.println("Taking readings:");
  display.display();
  delay(5000);

  for (int i = 0; i < numReadings; i++) {
    // Read sensor value
    int16_t fingerRawValue = ads.readADC_SingleEnded(0);

    // Calculate Absorbance (x_val)
    float absorbance = 0;
    if (fingerRawValue > 0) {
      absorbance = log10((float)baselineRawValue / fingerRawValue);
    }

    // Calculate glucose
    float glucose_mgdl;
    if (userGender == MALE) {
      glucose_mgdl = (male_a * pow(absorbance, 2)) + (male_b * absorbance) + male_c;
    } else {
      glucose_mgdl = (female_a * pow(absorbance, 2)) + (female_b * absorbance) + female_c;
    }

    readings[i] = glucose_mgdl;

    // Print to serial for monitoring the process
    Serial.print("Reading ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(glucose_mgdl);

    delay(200); // Wait between readings
  }

  // Step 2: Simplified K-Means Clustering and Averaging
  // Find the min and max to determine cluster centroids
  float minValue = readings[0];
  float maxValue = readings[0];
  for (int i = 1; i < numReadings; i++) {
    if (readings[i] < minValue) {
      minValue = readings[i];
    }
    if (readings[i] > maxValue) {
      maxValue = readings[i];
    }
  }

  // Initialize two centroids
  float centroid1 = minValue;
  float centroid2 = maxValue;

  // Declare sum and count variables here, outside the inner loop
  float sum1 = 0, count1 = 0;
  float sum2 = 0, count2 = 0;

  // Re-assign points to clusters and recalculate centroids
  for (int iter = 0; iter < 5; iter++) { // 5 iterations for convergence
    sum1 = 0;
    count1 = 0;
    sum2 = 0;
    count2 = 0;

    for (int i = 0; i < numReadings; i++) {
      if (abs(readings[i] - centroid1) < abs(readings[i] - centroid2)) {
        sum1 += readings[i];
        count1++;
      } else {
        sum2 += readings[i];
        count2++;
      }
    }

    if (count1 > 0) centroid1 = sum1 / count1;
    if (count2 > 0) centroid2 = sum2 / count2;
  }

  // Find the largest cluster and get its average
  float finalAverage;
  if (count1 > count2) {
    finalAverage = centroid1;
  } else {
    finalAverage = centroid2;
  }

  // Step 3: Display the final result
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Gender:");                                                         
  display.setCursor(50, 0);
  if (userGender == MALE) {
    display.println("MALE");
  } else {
    display.println("FEMALE");
  }

  display.setCursor(0, 20);
  display.println("Final Reading:");
  display.setCursor(0, 40);
  display.setTextSize( 1);
  display.print(finalAverage);
  display.println(" mg/dL");
  display.display();

  Serial.print("\nFinal Average Glucose: ");
  Serial.print(finalAverage);
  Serial.println(" mg/dL");

  delay(10000); // Wait for 1 minute before restarting
}