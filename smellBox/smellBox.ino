/**
  *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS 1    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required **
 * SPI SS 2    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required **
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 */

/*  Wiring up the RFID Readers ***
 *  RFID readers based on the Mifare RC522 like this one:  http://amzn.to/2gwB81z
 *  get wired up like this:
 *
 *  RFID pin    Arduino pin (above)
 *  _________   ________
 *  SDA          SDA - each RFID board needs its OWN pin on the arduino
 *  SCK          SCK - all RFID boards connect to this one pin
 *  MOSI         MOSI - all RFID boards connect to this one pin
 *  MISO         MISO - all RFID boards connect to this one pin
 *  IRQ          not used
 *  GND          GND - all RFID connect to GND
 *  RST          RST - all RFID boards connect to this one pin
 *  3.3V         3v3 - all RFID connect to 3.3v for power supply
 *
 */

#include <SPI.h>
#include <MFRC522.h>
//#include "espTcpClient.h"

#define RST_PIN 10 // Configurable, see typical pin layout above

//each SS_x_PIN variable indicates the unique SS pin for another RFID reader
#define SS_1_PIN 7 // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_2_PIN 9  // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1
#define SS_3_PIN 8  // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1
#define SS_4_PIN 6  // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1

#define LOCK_LIFT 2 //Lock that lifts Top

#define TCP_REQUEST_PERIOD_MS 5000
#define TCP_BLOCK_TIMEOUT_MS 1000

//this is the pin the relay is on, change as needed for your code

//must have one SS_x_PIN for each reader connected
#define NR_OF_READERS 4

//WiFi
unsigned long tcpRequestTS = 0;
unsigned long tcpTimeOut = 1000;

int sendErrorCount=0;
long lastCardDetected = 0;
bool _connected = false;
bool doorLocked = false;


//MFR5210
byte ssPins[] = {SS_1_PIN, SS_2_PIN, SS_3_PIN, SS_4_PIN};

MFRC522 mfrc522[NR_OF_READERS]; // Create MFRC522 instance.
String read_rfid;

//these are hard coded "right" card reads. You will have to change these to match
//cards you have in inventory - even better this code should be updated
//so that you can store new valid cards in EEProm.
String ValidCards[4][2] = {{"ed45ba79", "000"}, {"f9fb979", "0000"}, {"c97aba79", "0000"}, {"cb3cb879", "0000"}};
String ValidCard_1, ValidCard_2, ValidCard_3;
int Card_ok[] = {false, false, false, false};
int PuzzleState = 0;

int stageNumber = -2;

int waitForTCPRecieve = 0;

/**
 * Initialize.
 */
void setup()
{
  Serial.begin(115200); // Initialize serial communications with the PC

  pinMode(LOCK_LIFT, OUTPUT);
  digitalWrite(LOCK_LIFT, HIGH);
  
  
    ; // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  //espTcpClient_init();

  SPI.begin(); // Init SPI bus

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
  {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }
  PuzzleState = 1;
}

/**
 * Main loop.
 */
void loop()
{
  readRFID();

  delay(100);
}

void readRFID()
{

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
  {
    // Look for new cards
    byte bufferATQA[2];
    byte bufferSize = sizeof(bufferATQA);
    MFRC522::StatusCode status = mfrc522[reader].PICC_RequestA(bufferATQA, &bufferSize);
    bool result = (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION);
    if (!result)
    {
      status = mfrc522[reader].PICC_WakeupA(bufferATQA, &bufferSize);
      result = (status == MFRC522::STATUS_OK || status == MFRC522::STATUS_COLLISION);
    }
    if (status == 0 || status == 2)
    {
      Serial.print("<");
     // mfrc522[reader].PCD_SetAntennaGain(0x07);
      int c = 0;
      if (mfrc522[reader].PICC_ReadCardSerial())
        c++;
      if (mfrc522[reader].PICC_ReadCardSerial())
        c++;
      Serial.print(c);
      if (c > 0)
      {
        // Show some details of the PICC (that is: the tag/card)
        dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);

        Serial.print(F(", Reader "));
        Serial.print(reader);
        Serial.print(" ");
        Serial.print(read_rfid);

        ValidateCard(reader);
      }
      Serial.println(">");
    }
    else
    {
      if (Card_ok[reader] > 0)
        Card_ok[reader]--;
    }

    // Stop encryption on PCD
    mfrc522[reader].PCD_StopCrypto1();
    // Halt PICC
    mfrc522[reader].PICC_HaltA();
  }
}

void dump_byte_array(byte *buffer, byte bufferSize)
{
  read_rfid = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    read_rfid = read_rfid + String(buffer[i], HEX);
  }
}

void ValidateCard(int reader)
{
  //check each valid card
  bool found = false;
  int cardCount = 0;

  for (int j = 0; j < sizeof(ValidCards[reader]); j++)
  {
    if (ValidCards[reader][j] == read_rfid)
    {
      found = true;
      lastCardDetected = millis();
    }
  }
  if (found)
    Card_ok[reader] = 3;
  else
    Card_ok[reader]--;
    
  for (int j = 0; j < NR_OF_READERS; j++)
  {
    if (Card_ok[j] > 0)
      cardCount++;
  }

  if (cardCount == NR_OF_READERS-1 && lastCardDetected + 4000 < millis())
  {
    open_lift();
  }
  if (cardCount == NR_OF_READERS)
  {
    open_lift();
  }
  Serial.print(" Valid " + String(Card_ok[0]) + String(Card_ok[1]) + String(Card_ok[2]) + String(Card_ok[3]));
}

void open_lift()
{
  Serial.println("****************************************** LIFT opened");
  digitalWrite(LOCK_LIFT, LOW);
  PuzzleState = 2;
}
