/*
 * Antonio Cunyat Alario
 * toni at cunyat.net
 * This software is under GPL license.
 */

/*
  * This is a cli serial interface for mfrc522
  * It allows to copy, modify or clone cards using a serial interface commands.
  * It also allows to write block zero 
  * 
  * Commands: read, lka, show, lb, clean
  
  * command: lka (sector) (key)
  * description: load key A for a sector. This key will be used in read or write card commands.
  * parameter: sector number
  * parameter: key
  * example:
  *     lka 3 a0a1a2a3a4a5 (load key a0a1a2a3a4a5 into internal buffer for sector 3)
  * 
  * command: read (card/uid)
  * description: read to the buffer a mifare classic card using keys. default FFFFFFFFFFF
  * params: card / card
  * example:
  *   read card 
  *   reads a entire card 
  * 
  * command: write (zero)
  * writes data in the buffer to the card. To see what data is going to be written "show write"
  * 
  * command: lb (block number) (data)
  * description: loads to internal buffer the data for a specified block number 
  * parameter: block number
  * parameter: 16 bytes hex data in ascci mode 
  * example: 
  *     lb 4 fffebbccaabbcc112233445566aa1122
  * 
  * command: show [write] [keys] [uid]
  * description: show data
  * parameter: write - show blocks to write (all blocks if read command or several if lb command)
  * parameter: keys  - show loaded keys for read or write commands
  * parameter: uid   - show uid card (needed to aproach it to the reader)
  * example:
  *     show write
  *     show uid
  *     show keys
  * 
  * command: clear (data) (keys) [all]
  * description: clear stored buffer data 
  * example:
  *     clear data  -  clear readed or loaded data stored in buffer
  *     clear keys  -  clear all loaded keys to FFFFFFFFFFFF
  * 
  * Command: fix [card] (stop)
  *   fix start  try to fix unresponsive card
  *   fix stop  stop trying fix card

 
 */

// Inlcude Library
// MFRC522
#include <SPI.h>
#include <MFRC522.h>

//duemilanove
// #define SS_PIN 10
// #define RST_PIN 9

/*
 wemos d1 mini
RST  - D4
MISO - D6
MOSI - D7
SCK  - D5
SDA  - D8
*/
#define SS_PIN D8
#define RST_PIN D4

// SimpleCLI
#include <SimpleCLI.h>
#define COMMAND_QUEUE_SIZE 1
#define ERROR_QUEUE_SIZE 1
// DEBUG
//#include <MemoryFree.h>

// functions

void printHex(byte *buffer, byte bufferSize);
void printDec(byte *buffer, byte bufferSize);

void ShowReaderDetails();
void SetupCommands();  // configure commands
void SerialCommands(); // main function to get serial commands
void loadBlock(byte block, String data);
void loadKeyA(byte block, String blockData);
void authenticateBlock(int block); // Authenticate to block using key A

uint8_t readCardUID(); // get card UID
uint8_t isCardPresent();
void stopReader();
void writeCard();
void writeBlockZero();
void readCard();
void writeBlock();
void showKeys();
void initReadBlocks();
void initWriteBlocks();
void initKeys();
void showWrite();
void printOK();
void printERROR();
int ahex2int(char a, char b); // return a int from 2 char hex ie: FF = 255

// Variables
byte cardUID[4];
bool toReadUID = false;
bool toWriteCard = false;
bool toReadCard = false;
bool toFixCard = false;
bool toWriteBlockZero = false;     // to write the block zero can be dangerous and can brick your card
bool toWriteSectorTrailer = false; // to write a sector trailer can be dangerous and can brick your card

byte block;
byte data[64][16]; // data to write to the card
byte keys[16][6];  // keys to write

bool write_block[64]; //wich block should be written
bool read_block[64];

MFRC522::MIFARE_Key key;
MFRC522 mfrc522(SS_PIN, RST_PIN); // Instance of the class
MFRC522::StatusCode status;

// Init array that will store new NUID
byte nuidPICC[4];
uint8_t successRead;

// Create CLI Object
SimpleCLI serial_command(COMMAND_QUEUE_SIZE, ERROR_QUEUE_SIZE);

// Commands

Command cmdHelp;
Command cmdLoadBlock;
Command cmdLoadKeyA;
Command cmdWrite2Card;
Command cmdRead;
Command cmdFixCard;
Command cmdShow;
Command cmdClear;

void setup()
{
  Serial.begin(9600);
  Serial.println("");
  // Initialize variables
  initReadBlocks();
  initWriteBlocks();
  initKeys();

  // CLI
  SetupCommands();

  // write
  Serial.println("Started!");

  // Init RFID
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  /*   ShowReaderDetails();

  Serial.println(F("This code scan the MIFARE Classsic NUID."));
  Serial.print(F("Using the following key:"));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE); */
}

void loop()
{
  do
  {
    SerialCommands();
    if (toFixCard)
    {
      if (mfrc522.MIFARE_UnbrickUidSector(false))
      {
        printOK();
        toFixCard = false;
      }
    }
  } while (!isCardPresent());
  if (toReadUID)
  {
    readCardUID();
    toReadUID = false;
  }
  else if (toReadCard)
  {
    readCard();
    toReadCard = false;
  }
  else if (toWriteCard)
  {
    Serial.print(F("Writing card"));

    writeCard();
    toWriteCard = false;
  }
  else if (toWriteBlockZero)
  {
    writeBlockZero();
    toWriteBlockZero = false;
  }
}

void loadBlock(byte block, String blockData)
{
  if (blockData.length() == 32)
  {
    char data_array[33];
    byte block_value;
    blockData.toCharArray(data_array, 33);
    for (int i = 0; i < 31; i = i + 2)
    {
      block_value = ahex2int(data_array[i], data_array[i + 1]);
      data[block][i / 2] = (byte)block_value;
    }
    write_block[block] = true;
    printOK();
  }
  else
  {
    printERROR();
  }
}

void loadKeyA(byte sector, String blockData)
{
  if (blockData.length() != 12)
  {
    printERROR();
  }
  else
  {
    char data_array[13];
    byte block_value;
    blockData.toCharArray(data_array, 13);

    for (int i = 0; i < 11; i = i + 2)
    {
      block_value = ahex2int(data_array[i], data_array[i + 1]);
      keys[sector][i / 2] = (byte)block_value;
    }
    printOK();
  }
}

void ShowReaderDetails()
{
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF))
  {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    while (true)
      ; // do not go further
  }
}

uint8_t isCardPresent()
{
  // Getting ready for Reading PICCs
  if (!mfrc522.PICC_IsNewCardPresent())
  { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial())
  { //Since a PICC placed get Serial and continue
    return 0;
  }
  return 1;
}

///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t readCardUID()
{
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  for (uint8_t i = 0; i < 4; i++)
  { //
    cardUID[i] = mfrc522.uid.uidByte[i];
    Serial.print(cardUID[i], HEX);
  }
  Serial.println("");
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void authenticateBlock(int block)
{

  byte sector = (int)(block / 4);
  //read keys sector
  for (int j = 0; j < 6; j++)
  {
    key.keyByte[j] = keys[sector][j];
  }
  // delay(5);
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK)
  {
    Serial.println(block);

    printERROR();
    mfrc522.PICC_HaltA(); // Stop reading
  }
}

void readCard()
{
  Serial.println(F("Read CARD:"));
  byte buffer[18];

  for (int block = 0; block < 64; block++)

  {
    if (block % 4 == 3)
    {
      continue; //trailer blocks
    }
    authenticateBlock(block);

    // Read block
    byte byteCount = sizeof(buffer);
    status = mfrc522.MIFARE_Read(block, buffer, &byteCount);
    if (status != MFRC522::STATUS_OK)
    {
      printERROR();
      mfrc522.PICC_HaltA(); // Stop reading
      mfrc522.PCD_StopCrypto1();

      return;
      // Serial.println(F("MIFARE_Read() failed: "));
      // Serial.println(mfrc522.GetStatusCodeName(status));
    }
    else
    {
      // Serial.println(F("Success with key:"));
      for (int p = 0; p < 16; p++)
      {
        data[block][p] = buffer[p];
        write_block[block] = true;
      }
    }
  }
  printOK();
  mfrc522.PICC_HaltA(); // Stop reading
  mfrc522.PCD_StopCrypto1();
}

void writeCard()
{
  Serial.println(F("Write CARD:"));

  if (toWriteBlockZero && write_block[0] == true)
  {
    writeBlockZero();
    write_block[0] = false;
  }
  for (int block = 1; block < 64; block++)
  {
    if (write_block[block] == false)
    {
      continue;
    }
    if ((block % 4) == 3 and toWriteSectorTrailer == false)
    {
      continue;
    }

    authenticateBlock(block);
    delay(10);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(block, data[block], 16);
    if (status != MFRC522::STATUS_OK)
    {
      printERROR();
      // Serial.println(mfrc522.GetStatusCodeName(status));
      mfrc522.PICC_HaltA(); // Halt PICC
      mfrc522.PCD_StopCrypto1();
      return;
    }
  }
  printOK();
  mfrc522.PICC_HaltA(); // Halt PICC
  mfrc522.PCD_StopCrypto1();
}

void writeBlockZero()
// TODO
{
  Serial.println(F("Write BlockZero:"));
  bool logErrors = true;
  authenticateBlock(block);

  byte block0_buffer[18];
  mfrc522.PCD_StopCrypto1();
  if (!mfrc522.MIFARE_OpenUidBackdoor(logErrors))
  {
    if (logErrors)
    {
      printERROR();
    }
  }
  for (uint8_t i = 0; i < 16; i++)
  {
    block0_buffer[i] = data[0][i];
  }

  // Write modified block 0 back to card
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Write((byte)0, block0_buffer, (byte)16);
  if (status != MFRC522::STATUS_OK)
  {
    printERROR();
  }

  // Wake the card up again
  /*   byte atqa_answer[2];
  byte atqa_size = 2;

  mfrc522.PICC_WakeupA(atqa_answer, &atqa_size); */
  authenticateBlock(0);
}

// data conversion functions

// return an int from 2 char hex
int ahex2int(char a, char b)
{

  a = (a <= '9') ? a - '0' : (a & 0x7) + 9;
  b = (b <= '9') ? b - '0' : (b & 0x7) + 9;

  return (a << 4) + b;
}

void printOK()
{
  Serial.println("OK");
}
void printERROR()
{
  Serial.println("ERROR");
}

// Print functions
void showWrite()
{
  for (int block_number = 0; block_number < 64; block_number++)
  {
    if (write_block[block_number] == true)
    {
      Serial.print(block_number);
      Serial.print(" ");
      printHex(data[block_number], 16);
    }
    delay(10); // Issues with Serial.print
  }
}

void showKeys()
{
  for (int sector = 0; sector < 16; sector++)
  {
    printHex(keys[sector], 6);
  }
}

/* *
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? "0" : "");
    Serial.print(buffer[i], HEX);
  }
  Serial.println("");
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

void SetupCommands()
{
  // Commands
  // Help

  cmdHelp = serial_command.addCommand("help");

  cmdShow = serial_command.addCommand("show");
  cmdShow.addPosArg("");
  cmdClear = serial_command.addCommand("clear");
  cmdClear.addPosArg("", "all");
  // Load Block
  cmdLoadBlock = serial_command.addCommand("lb");
  //cmdLoadBlock.setDescription("load a block -b of -d 16 bytes data");
  cmdLoadBlock.addPosArg("b");
  cmdLoadBlock.addPosArg("d");
  // Load key A
  cmdLoadKeyA = serial_command.addCommand("lka");
  //cmdLoadKeyA.setDescription("load a write key -b of sector -k");
  cmdLoadKeyA.addPosArg("b");
  cmdLoadKeyA.addPosArg("k");
  cmdWrite2Card = serial_command.addCommand("write");
  cmdWrite2Card.addPosArg("");
  cmdRead = serial_command.addCommand("read");
  cmdRead.addPosArg("", "uid");
  cmdFixCard = serial_command.addCommand("fix");
  cmdFixCard.addPosArg("", "card");
}
void SerialCommands()
{

  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');

    if (input.length() > 0)
    {
      Serial.print("# ");
      Serial.println(input);
      serial_command.parse(input);
    }
  }

  if (serial_command.available())
  {
    Command c = serial_command.getCmd();
    Serial.print("> ");
    Serial.print(c.getName());
  

    Serial.println();
    //Get 2 arguments
    Argument arg1 = c.getArgument(0);
    Argument arg2 = c.getArgument(1);

    if (c == cmdHelp)
    {
      Serial.println("Help:");
      Serial.println(serial_command.toString());
    }
    else if (c == cmdLoadBlock)
    {

      String tmpblock = arg1.getValue();
      int block = atoi(tmpblock.c_str());
      String blockData = arg2.getValue();
      loadBlock(block, blockData);
      // printOK();
    }
    else if (c == cmdLoadKeyA)
    {
      // loads keya from serial input
      String str_block = arg1.getValue();
      int block = atoi(str_block.c_str());
      String blockData = arg2.getValue();
      loadKeyA(block, blockData);
    }
    else if (c == cmdShow)
    {
      if (arg1.getValue() == "write")
      {
        showWrite();
      }
      else if (arg1.getValue() == "keys")
      {
        showKeys();
      }
    }
    else if (c == cmdFixCard)

    {
      if (arg1.getValue() == "start")
      {
        toFixCard = true;
      }
      else if (arg1.getValue() == "stop")
      {
        toFixCard = false;
      }
    }

    else if (c == cmdClear)

    {

      if (arg1.getValue() == "keys")
      {
        initKeys();
      }
      else if (arg1.getValue() == "data")
      {
        initWriteBlocks();
      }
      else if (arg1.getValue() == "all")
      {
        initKeys();
        initWriteBlocks();
      }
    }
    else if (c == cmdRead)
    {
      if (arg1.getValue() == "card")
      {
        toReadCard = true;
      }
      if (arg1.getValue() == "uid")
      {
        toReadUID = true;
      }
    }
    else if (c == cmdWrite2Card)
    {
      toWriteCard = true;
      if (arg1.getValue() == "zero")
      {
        toWriteBlockZero = true;
      }
      if (arg1.getValue() == "trailer")
      {
        toWriteSectorTrailer = true;
      }
    }
    else
    {
      Serial.println("ERROR: Command not defined");
    }
  }

  /*    if (serial_command.errored()) {
        CommandError cmdError = serial_command.getError();

        Serial.print("ERROR: ");
        Serial.println(cmdError.toString());

        if (cmdError.hasCommand()) {
            Serial.print("Did you mean \"");
            Serial.print(cmdError.getCommand().toString());
            Serial.println("\"?");
        }
    } */
}

void initWriteBlocks()
{
  for (int block = 0; block < 64; block++)
  {
    write_block[block] = false;
  }
}

void initReadBlocks()
{
  for (int block = 0; block < 64; block++)
  {
    read_block[block] = false;
  }
}
void initKeys()
{
  for (int sector = 0; sector < 16; sector++)
  {
    for (int field = 0; field < 6; field++)
    {
      keys[sector][field] = 0xFF;
    }
  }
}
