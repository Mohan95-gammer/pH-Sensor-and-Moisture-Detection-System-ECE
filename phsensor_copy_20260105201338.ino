#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD setup (I2C address 0x27, 16x2 display)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// BUTTONS
#define BTN_NEXT   2
#define BTN_SELECT 3
const unsigned long LONG_PRESS_TIME = 1500;
const unsigned long DEBOUNCE_DELAY  = 80;

// PH SENSOR
const int phPin = A0;

// FIXED PH RANGE
float pH_low  = 7.0;
float pH_high = 11.0;

// Calibration values (update with your measured voltages)
float voltage_pH7  = 1.15;   // voltage at pH7
float voltage_pH11 = 1.85;   // voltage at pH11

// MOISTURE SENSOR
const int moistPin = A1;
const int moistMin = 300;
const int moistMax = 660;

// CROPS DATA
String crops[4] = {"Rice","Wheat","Tomato","AloeV"};
int cropMin[4]  = {60,40,50,20};
int cropMax[4]  = {90,70,80,40};

int cropIndex   = 0;
bool cropChosen = false;

// CUSTOM CHARACTERS
byte checkIcon[8] = {0b00000,0b00001,0b00011,0b10110,0b11100,0b01000,0b00000,0b00000};
byte crossIcon[8] = {0b00000,0b10001,0b01010,0b00100,0b01010,0b10001,0b00000,0b00000};

// ====================== NON-BLOCKING BUTTON ========================
bool isPressed(int pin){
  static unsigned long lastTime[10];
  static int lastState[10];

  int reading = digitalRead(pin);
  if(reading != lastState[pin]) {
    lastTime[pin] = millis();
  }

  if(millis() - lastTime[pin] > DEBOUNCE_DELAY) {
    lastState[pin] = reading;
    return (reading == LOW);
  }
  return false;
}

// ====================== MEDIAN FILTER pH ============================
int readMedianPH() {
  const int N = 15;
  int vals[N];

  for (int i=0; i<N; i++){
    vals[i] = analogRead(phPin);
    delay(4);
  }

  for (int i=0; i<N-1; i++)
    for (int j=i+1; j<N; j++)
      if(vals[j] < vals[i]) {
        int temp = vals[i];
        vals[i] = vals[j];
        vals[j] = temp;
      }

  return vals[N/2];
}

// Compute pH using linear interpolation across 7–11
float computePH(float voltage){
  float slope = (pH_high - pH_low) / (voltage_pH11 - voltage_pH7);
  float pH = pH_low + slope * (voltage - voltage_pH7);
  return constrain(pH, pH_low, pH_high);
}

// ====================== MOISTURE ========================
int readMoisture() {
  int raw = analogRead(moistPin);
  float m = 100.0 * (moistMax - raw) / (moistMax - moistMin);
  return constrain((int)m, 0, 100);
}

// ====================== SETUP ========================
void setup() {
  Serial.begin(9600);

  lcd.init();        // initialize LCD
  lcd.backlight();   // turn on backlight

  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  lcd.createChar(0, checkIcon);
  lcd.createChar(1, crossIcon);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("  Select Crop");
  lcd.setCursor(0,1);
  lcd.print("-> ");
  lcd.print(crops[cropIndex]);
}

// ====================== LOOP ==========================
void loop() {
  if(!cropChosen) handleMenu();
  else showReadings();
}

// ====================== MENU ==========================
void handleMenu(){
  static int lastIndex = -1;

  if(lastIndex != cropIndex){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("  Select Crop");
    lcd.setCursor(0,1);
    lcd.print("-> ");
    lcd.print(crops[cropIndex]);
    lastIndex = cropIndex;
  }

  if(isPressed(BTN_NEXT)){
    cropIndex = (cropIndex + 1) % 4;
  }

  if(isPressed(BTN_SELECT)){
    cropChosen = true;
    lcd.clear();
    lcd.setCursor(0,0); lcd.print(" Selected:");
    lcd.setCursor(0,1); lcd.print(" ");
    lcd.print(crops[cropIndex]);
    lcd.write(byte(0));
    delay(1200);
  }
}

// ====================== SENSOR SCREEN ==========================
void showReadings(){
  static unsigned long pressStart = 0;
  static bool pressing = false;

  // ---- pH ----
  int phRaw = readMedianPH();
  float voltage = phRaw * (5.0 / 1023.0);
  float pHValue = computePH(voltage);

  Serial.print("pH Raw: "); Serial.print(phRaw);
  Serial.print("  Voltage: "); Serial.print(voltage);
  Serial.print("  pH: "); Serial.println(pHValue);

  // ---- Moisture ----
  int moisture = readMoisture();
  bool suitable = (moisture >= cropMin[cropIndex] && moisture <= cropMax[cropIndex]);

  // ---- LCD ----
  lcd.setCursor(0,0);
  lcd.print("pH: ");
  lcd.print(pHValue,1);
  lcd.print("   ");

  lcd.setCursor(10,0);
  lcd.print(crops[cropIndex].substring(0,6));

  lcd.setCursor(0,1);
  lcd.print("M:");
  lcd.print(moisture);
  lcd.print("% ");

  if(moisture < cropMin[cropIndex]) lcd.print("Low ");
  else if(moisture > cropMax[cropIndex]) lcd.print("High");
  else lcd.print("Good");

  lcd.setCursor(15,1);
  lcd.write(suitable ? 0 : 1);

  // ---- LONG HOLD SELECT → MENU ----
  if(digitalRead(BTN_SELECT)==LOW){
    if(!pressing){
      pressing = true;
      pressStart = millis();
    } else if(millis() - pressStart > LONG_PRESS_TIME){
      cropChosen = false;
      pressing = false;
      lcd.clear();
      lcd.print(" Returning...");
      delay(800);
      lcd.clear();
      return;
    }
  } else {
    pressing = false;
  }

  delay(200);
}