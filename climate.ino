#include <dht.h>    // Humid & Temperature mätinstrument
#include <LiquidCrystal.h>  // LCD-display


  // en.wikipedia.org/wiki/File:Relative_Humidity.png
  // Antalet gram  vatten per kg luft, m, som funktion av procent luftfuktighet, RH, och temperatur, T, grader celsius
  // m = RH * 0,0407 * exp(T * 0,063)

#define UINT  unsigned long    // 32-bit unsigned integer 0 <= UINT < 4294967296
#define FLYTTAL  double        // den typ av flyttal som ska användas genom hela
#define DHT22_PIN 6    // Pin kopplad till kommunikation med H&T-mätaren
#define REL_HUMID 8    // Pin kopplad till relä för luftfuktaren
#define REL_HEAT 9     // Pin kipplad till relä för värmeaggregatet
#define MONITOR_PRINT false  // sann om data ska loggas på dataskärmen
#define TRUNC_VAL  0.8 // Någon form av trunkeringsmetod för att slippa enskilda mätvärden som är åt skogen
#define CHECK_FREQUENCY  600

#define MAX_TEMP  70.0    // Stäng ner allt permanent om temperaturen överstiger MAX_TEMP
#define MAX_HUMID 90.0    // - || - överstiger MAX_HUMID
#define OPTIMAL_TEMP  45.0  // låt temperaturen ligga kring detta värde
#define OPTIMAL_HUMID  70.0  // låt relativa luftfuktigheten ligga kring detta värde
#define SIGMA_HEAT  5.0    // 
#define SIGMA_HUMID  5.0
#define DT_DT  ((FLYTTAL) 0.02)    // temperaturökning per tidsenhet dTemp / dTid
#define DH_DT  ((FLYTTAL) 0.01)    // luftfuktighetsökning per tidsenhet dHumid / Tid


  // forum.arduino.cc/index.php?topic=19002.0
#define DEG  ((char)223)
  
dht DHT;
LiquidCrystal lcd(12, 11, 10, 5, 4, 3, 2);

struct {
  boolean distressAlarm;
  
  boolean heatActive;
  UINT setHeatActiveUntil;
  UINT checkHeatActiveTime;
  
  boolean humidActive;
  UINT setHumidActiveUntil;
  UINT checkHumidActiveTime;

  FLYTTAL tempValue;
  FLYTTAL humidValue;
  
  int err;
} state = {false, false, 0, 0, false, 0, 0, OPTIMAL_TEMP, OPTIMAL_HUMID};

void setHeatActive(boolean mode, FLYTTAL until)
{
  if (mode == true && state.heatActive == false) {
    digitalWrite(REL_HEAT, HIGH);
  } else if (mode == false)
    digitalWrite(REL_HEAT, LOW);
  state.heatActive = mode;  
  state.setHeatActiveUntil = until;
  state.checkHeatActiveTime = until + CHECK_FREQUENCY;
  if (MONITOR_PRINT) {  
    if (mode == true)
      Serial.println("Set Heat Active");
    else
      Serial.println("Set Heat Inactive");
  }
}


void setHumidActive(boolean mode, FLYTTAL until)
{
  if (mode == true && state.humidActive == false){
    digitalWrite(REL_HUMID, HIGH);
  } else if (mode == false)
    digitalWrite(REL_HUMID, LOW);
  state.humidActive = mode;  
  state.setHumidActiveUntil = until;
  state.checkHumidActiveTime = until + CHECK_FREQUENCY;
  if (MONITOR_PRINT) {  
    Serial.println("");
    if (mode == true)
      Serial.println("Set Humid Active");
    else
      Serial.println("Set Humid Inactive");
  }
}

void updateSettings(FLYTTAL nyHum, FLYTTAL nyTemp)
{
      // kolla efter felaktigheter och om något är fuffens så stängs allt ner.
  if (nyHum < 0 || (nyHum > MAX_HUMID && (nyTemp>OPTIMAL_TEMP-SIGMA_HEAT)) || nyTemp < -10 || nyTemp > MAX_TEMP)
    state.distressAlarm = true;

  UINT timeNow = getSecondsPassed();
  
  if (state.distressAlarm) {
    if (state.heatActive)
      setHeatActive(true, timeNow);
    if (state.humidActive)
      setHumidActive(true, timeNow);
    return;
  }
  
  state.humidValue += (nyHum - state.humidValue)*TRUNC_VAL;
  state.tempValue += (nyTemp - state.tempValue)*TRUNC_VAL;  
      
  if (state.tempValue > OPTIMAL_TEMP+SIGMA_HEAT) {    // Om temperaturen har gått över normalintervall
    if (MONITOR_PRINT)    Serial.println("Temp över normalintervall");
    if (state.heatActive == true)
      setHeatActive(false, timeNow);
    if (state.humidActive == true)
      setHumidActive(false, timeNow);
    state.checkHeatActiveTime = timeNow + CHECK_FREQUENCY;
    state.checkHumidActiveTime = timeNow + CHECK_FREQUENCY;
    
  } else if (state.tempValue < OPTIMAL_TEMP-SIGMA_HEAT) {  // Om temperaturen har understigit normalintervall
    if (MONITOR_PRINT)    Serial.println("Temp under normalintervall");
    if (state.humidActive == true)  // stäng av humid
      setHumidActive(false, timeNow); 
    if (state.heatActive == false)  // sätt på värmeaggregatet
      setHeatActive(true, timeNow + min((OPTIMAL_TEMP - state.tempValue) / DT_DT, CHECK_FREQUENCY));
      
  } else {      // Om temperaturen är inom normalintervall
    if (MONITOR_PRINT)    Serial.println("Temp inom normalintervall");
    if (timeNow > state.checkHeatActiveTime)
    {
      if (state.tempValue < OPTIMAL_TEMP-SIGMA_HEAT/2) {
        setHeatActive(true, timeNow + min(2*(OPTIMAL_TEMP - state.tempValue) / DT_DT, CHECK_FREQUENCY));
      }
    } else if (timeNow > state.setHeatActiveUntil && state.heatActive){
      setHeatActive(false, timeNow);
    }
    
    if (timeNow > state.checkHumidActiveTime)
    { 
      if (state.humidValue < OPTIMAL_HUMID-SIGMA_HUMID) {
        setHumidActive(true, timeNow + min((OPTIMAL_HUMID - state.humidValue) / DT_DT, CHECK_FREQUENCY));
      } else if (state.humidValue < OPTIMAL_HUMID-SIGMA_HUMID/2) {
        setHumidActive(true, timeNow + min(2*(OPTIMAL_HUMID - state.humidValue) / DT_DT, CHECK_FREQUENCY));
      }
      
      if (state.humidValue > OPTIMAL_HUMID+SIGMA_HUMID/2)
      {
        setHumidActive(false, timeNow);
      }
    } else if (timeNow > state.setHumidActiveUntil && state.humidActive) {
      setHumidActive(false, timeNow);
    }
  }
}

UINT getSecondsPassed()
{
  static UINT lastValue = 0;
  static UINT timePeriods = 0;
  UINT thisValue = micros();
  FLYTTAL fTime;

  if (lastValue > thisValue)  {    
    timePeriods++;  
  }
  lastValue = thisValue;
  fTime = (FLYTTAL) timePeriods * 4294967296.0 + thisValue;
  return (UINT) (fTime * .000001);
}

void updateLCD(UINT tid) 
{
  lcd.setCursor(0, 0);
  lcd.print(DHT.temperature);
  lcd.setCursor(5, 0);
  char deg[5] = {DEG, 'C', ' ', ' ', ' '};
  lcd.print(deg);
  
  lcd.setCursor(10,0);
  lcd.print(DHT.humidity);
  lcd.setCursor(15,0);
  lcd.print("% RH ");
  
  lcd.setCursor(0, 1);
  lcd.print("t: ");
  lcd.setCursor(3, 1);
  if (tid < 1000000)
    lcd.print(tid);  
  else {
    lcd.print(tid/1000);
    lcd.print("k");
  }
  
  lcd.setCursor(10,1);
  lcd.print("{");
  lcd.print(state.heatActive? "T": " ");
  lcd.print(state.humidActive? "H": " ");
  lcd.print(state.distressAlarm? "A": " ");
  switch(state.err) {
    case DHTLIB_ERROR_CHECKSUM:
      lcd.print("C");
      break;
    case DHTLIB_ERROR_TIMEOUT:
      lcd.print("O");
      break;
    default:
      lcd.print(" ");
      break;
  }
  lcd.print("}");
}

void printStatus(UINT timeNow)
{
  Serial.print("Time: ");
  Serial.println(timeNow);
  Serial.print("DistressAlarm: ");
  Serial.println(state.distressAlarm? 'T': 'F');
  Serial.print("Temp: ");
  Serial.print(state.tempValue);
  Serial.print("\t Humid: ");
  Serial.println(state.humidValue);
  
  Serial.print("Heat active: ");
  Serial.println(state.heatActive? 'T': 'F');
  Serial.print("\t until: ");
  Serial.print(state.setHeatActiveUntil);
  Serial.print(" \tcheck: ");
  Serial.println(state.checkHeatActiveTime);
  
  Serial.print("Humid active: ");
  Serial.println(state.humidActive? 'T': 'F');
  Serial.print(" \tuntil: ");
  //Serial.print(timeNow < state.setHumidActiveUntil? state.setHumidActiveUntil-timeNow: timeNow - state.setHumidActiveUntil);
  Serial.print(state.setHumidActiveUntil);
  Serial.print(" \tcheck: ");
  Serial.println(state.checkHumidActiveTime);
  Serial.print("err: ");
  Serial.println(state.err);
  Serial.println("");
}

void setup()
{
  if (MONITOR_PRINT) {
    Serial.begin(9600);
    Serial.println("setup begin");
  }
  lcd.begin(20, 2);
  pinMode(REL_HUMID, OUTPUT);
  pinMode(REL_HEAT, OUTPUT);
  digitalWrite(REL_HUMID, LOW);
  digitalWrite(REL_HEAT, LOW);
  
  if (MONITOR_PRINT)      Serial.println("setup complete");
}

void loop()
{
  // READ DATA
  if (MONITOR_PRINT)   Serial.println("");
  state.err = DHT.read22(DHT22_PIN);
  updateSettings(DHT.humidity, DHT.temperature);
  UINT timeNow = getSecondsPassed();
  updateLCD(timeNow);
  if (MONITOR_PRINT)    printStatus(timeNow);
  delay(4000);
}
