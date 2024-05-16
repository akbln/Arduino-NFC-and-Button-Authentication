#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

#define SS_PIN 10
#define RST_PIN 9
#define servoPin 8
#define red A3
#define green A4
#define blue A5
#define pz 5


int tries = 0;

MFRC522 rfid(SS_PIN, RST_PIN);

Servo servo;

MFRC522::MIFARE_Key key; 

const byte hexKey[] = {0x13 , 0xAB, 0x4D, 0x1C};
const byte combinationKey[] = {1,2,2,1,3,3};

boolean isDoorLocked = false;

void setup() { 
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  servo.attach(servoPin);
  lockDoor();
  for(int i = 2; i < 6;i++){
    pinMode(i,INPUT);
  }
  // Using default key
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}
 
void loop() {
  // Make sure card is present
  if (!rfid.PICC_IsNewCardPresent()){
    return;
  }
  // Make sure card is readable
  if (!rfid.PICC_ReadCardSerial()){
    return;
  }

  // Serial.print(F("PICC type: "));

  // Obtain type of card
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  // Serial.println(rfid.PICC_GetTypeName(piccType));

  // Only support mifare clasic cards 
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }
  //Compare UIDs
  if(compareUID(hexKey,rfid.uid.uidByte,rfid.uid.size)){
    if(readButtons()){
      tries=0;
      // confirmation tone
      playPiezo(200, 1000);
      delay(50);
      playPiezo(200, 800);

      unlockDoor();
      // Wait for button press to close lock
      while(digitalRead(6)==LOW);
      lockDoor();
    }
    else{
       // Fail combination and reset led to red
      writeRGB(255,0,0);
      playPiezo(500,200);
      failUnlock();
    }
  }
  else{
    // Fail UID
      playPiezo(500,200);
      failUnlock();
  }
  rfid.PICC_HaltA();

  rfid.PCD_StopCrypto1();
}


//compares UID from library however since my key is 4 hex bytes will reject a card with more or less bytes as UID
bool compareUID(byte *StoredUID, byte *buffer, byte bufferSize){
  if(bufferSize != 4){
    return false;
  }
  for(byte i = 0; i < 4; i++){
    if(StoredUID[i] != buffer[i]){
      return false;
    }
  }
  return true;
}

void unlockDoor(){
  if(isDoorLocked){
     //aligned servo to be 10deg perfectly horizental not 0 deg
    servo.write(10);
    writeRGB(0,255,0);
    isDoorLocked = false;
  }
  
}
void lockDoor(){
  if(!isDoorLocked){
    //aligned servo to be 110deg perfectly horizental not 90 deg
    servo.write(110);
    writeRGB(255,0,0);
    isDoorLocked = true;
  }
}
void writeRGB(int redValue, int greenValue, int blueValue){
  analogWrite(red,redValue);
  analogWrite(green,greenValue);
  analogWrite(blue,blueValue);
}
boolean readButtons(){
  //Index to be written into in the array
  byte index = 0;

  // Default combination to be overriden by button inputs
  byte combination[6] = {0,0,0,0,0,0};

  // Timeout is 20 Seconds in the fututre
  unsigned long timeout = millis()+10000;
  unsigned long time = millis();
  unsigned long currentTime = time;
  boolean state = true;
  writeRGB(0,255,0);
  // To ensure that the loop doesnt go forever time which is updated in the loop and used elsewhere
  // Is compared with timeout which is 20 seconds in the fututre from when the loop began
  while(index < 6 && time < timeout){
    time = millis();
    //In order to effictivly blink the green led while also not blocking code execution
    //Time is compared to "current time" which is a value set to loop start time
    //And "current time" is only updated to the actual current time right now -> "time" variable which is updated each loop cycle, exceeds "currentTime" by 1 second
    //Effeectivly putting the green LED to blink on a 1second cycle without blocking code exec
    // State variable is to track the current state wether green led is on or off in order to do the opposite next itteration
    if(time > currentTime + 1000){
      if(state){
        writeRGB(0,0,0);
      }else{
        writeRGB(0,255,0);
      }
      state = !state;
      currentTime = millis();
    }
    // Any of these will write the code to the button pressed and then iterate index, only one write allowed per cycle.
    if(digitalRead(2)==HIGH){
        Serial.print(1);
        combination[index]=1;
        index++;
        playPiezo(250,500);
    }
    else if(digitalRead(3)==HIGH){
        Serial.print(2);
        combination[index]=2;
        index++;
        playPiezo(250,500);
    }
    else if(digitalRead(4)==HIGH){
        Serial.print(3);
        combination[index]=3;
        index++;
        playPiezo(250,500);
    }
    else if(digitalRead(5)==HIGH){
        Serial.print(4);
        combination[index]=4;
        index++;
        playPiezo(250,500);
    }else;
  }
  // once loop is done will compare preset key with what ever this loop manages to overwrite into its array
  return compareCombination(combination,combinationKey);
}

// Compare combination static size and will be called on 6 number array stored in memmory
bool compareCombination(byte *arr1, byte *arr2) {
    for (byte i = 0; i < 6; i++) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

void blinkRed(unsigned long d){
  // Blink Red LED for set amount of time
  // Divide by 2000 to evenly space 1000ms for turn on and off
  int blinkTimes = d/2000;
  while(blinkTimes > 0){
    writeRGB(255,0,0);
    delay(1000);
    writeRGB(0,0,0);
    delay(1000);
    blinkTimes--;
  }
  writeRGB(255,0,0);
}

//Effectivly locks out any tries if failed attempts exceed 5 due to blocking code on purpous of blinkRed unlike blinking green LED
void failUnlock(){
  tries = tries+1;
  if(tries >= 5){
    playPiezo(1600, 300);
    blinkRed(16000);
    tries = 0;
  }
}
// Helper function to play piezo at a frequency for a certain time
void playPiezo(unsigned long time,short freq){
  tone(pz,freq);
  delay(time);
  noTone(pz);
}





