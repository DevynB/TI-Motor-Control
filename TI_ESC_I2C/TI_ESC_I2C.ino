/* TI_ESC_I2C control
 * Author: Devyn Byrnes
 * Version: 1.0
 *  
 *  The TI motor controller HW created by GTK is controlled via I2C commands. 
 *  Please refer to the I2C register map for more detailed control information.
 *  This driver can control the ESC via I2C from an Arduino and terminal commands
 *  
 *  How to use:
 *  1. connect I2C_SDA, I2C_SCL and GND from arduiono to TI motor controller (you can also power motor controller from 3v3 output of arduiono)
 *  2. upload this sketch
 *  3. open a serial com terminal with settings (add Newline and 115200 baud)
 *  4. type "help" to see all commands.
 *  5. type whatever command you need.
 *  
 *  How to port to other sketches:
 *  1. you will need to include Wire.H 
 *  2. copy : wire setup in setup function
 *  3. copy : I2C register addresses
 *  3. copy : bool write_i2c(byte reg, word val) function
 *  4. copy : word read_i2c(byte reg)
 *  5. use the read and write functions to control the TI ESC
 * 

 */

#include <Wire.h>

/* Buffer for the incoming data */
char inData[100];
/* Buffer for the parsed data chunks */
char *inParse[100];

/* Storage for data as string */
String inString = "";

/* Incoming data id */
int index = 0;
/* Read state of incoming data */
bool stringComplete = false;

char incomingByte[2] = {0,0};          // for incoming serial data
byte error;
bool startlogging = 0;

/* I2C Register Addresses */
byte Motor_i2c_address = 44;            // Set Motor I2C address

byte Reg_FW_Ver_Addr      = 0x00;
byte Reg_Set_I2C_Addr     = 0x01;
byte Reg_Sys_Config_Addr  = 0x02;
byte Reg_Set_RPM_Addr     = 0x03;
byte Reg_Read_RPM_Addr    = 0x04;
byte Reg_Read_Current     = 0x05;
byte Reg_Read_Temp        = 0x06;

/* Constants */
bool Rotate_CCW           = 1; 
bool Rotate_CW            = 0; 
bool ENABLE               = 1;
bool DISABLE              = 0;
char WAIT_TIME            = 6; //initial calibration wait time seconds

/* Commands */
String readFW             = "ver";
String enable_motor       = "start";
String disable_motor      = "stop";
String readTemp           = "temp";
String readRPM            = "rpm";
String readCurrent        = "current";
String setRPM             = "rpm=";
String help               = "help";
String stat               = "stat";
String logstart           = "log=start";
String logstop            = "log=stop";


void setup()
{
  Wire.begin();                     // join i2c bus (address optional for master)
  
  Serial.begin(115200);             // Serial bit rate
  
  Serial.println("TI Motor Control V1:\n\n");
  
  delay(100);
  
  pinMode(8, OUTPUT);               // set pin to input
  
  digitalWrite(8, HIGH);            // turn on pullup resistors

  printCommands();
}

void loop()
{ 

  // Loop until a completed serial command is sent from terminal.
  if (stringComplete) 
  {
    // Parse the recieved data and execute functions
    ParseSerialData();
    // Reset inString to empty
    inString = "";    
    // Reset the system for further 
    // input of data   
    stringComplete = false; 
  }  
}

void serialEvent() 
{
  // Read while we have data
  while (Serial.available() && stringComplete == false) 
  {
    // Read a character
    char inChar = Serial.read(); 
    // Store it in char array
    inData[index] = inChar; 
    // Increment where to write next
    index++;     
    // Also add it to string storage just
    // in case, not used yet :)
    inString += inChar;
    
    // Check for termination character
    if (inChar == '\n') 
    {
      // Reset the index
      index = 0;
      // Set completion of read to true
      stringComplete = true;

      if (inString == (logstop+ "\n")){startlogging = 0;} //logging not finished.
    }
  }
}

//Logging not finished
void StartLog()
{
  // set toggle external trigger 
  digitalWrite(8, LOW);          
  digitalWrite(8, HIGH);     
  startlogging = 1;
  
  // counter
  int p = 0;                     

  // set RPM to 0
  write_i2c(Reg_Set_RPM_Addr,0); 

  // read and output to terminal
  for (p = 0; p<9000; p++)       
  {
      word read_data = read_i2c(Reg_Read_Current);
      Serial.println(read_data, DEC);  
      write_i2c(Reg_Set_RPM_Addr,p);
      delay(1);
  }

  // Reset RPM and external trigger
  write_i2c(Reg_Set_RPM_Addr,0);
  digitalWrite(8, LOW);       // turn on pullup resistors
}

void ParseSerialData()
{
  Serial.println("");
  if      (inString == (readFW + "\n"))         { Serial.print("FW version : ");   Serial.print(read_i2c(Reg_FW_Ver_Addr),DEC);}
  else if (inString == (enable_motor + "\n"))   { write_i2c(Reg_Set_RPM_Addr,0x0666); delay(30); En_Motor(ENABLE,Rotate_CCW);    delay(30); write_i2c(Reg_Set_RPM_Addr,0x0666);  delay(30);En_Motor(DISABLE,0x00); delay(30);En_Motor(ENABLE,Rotate_CCW);}
  else if (inString == (disable_motor + "\n"))  { En_Motor(DISABLE,0x00) ;    delay(30);     write_i2c(Reg_Set_RPM_Addr,0x0000);}
  else if (inString == (readTemp + "\n"))       { Serial.print("Temp (C) : ");     Serial.print(read_i2c(Reg_Read_Temp),DEC);}
  else if (inString == (readRPM + "\n"))        { Serial.print("RPM  : ");         Serial.print(read_i2c(Reg_Read_RPM_Addr),DEC);}
  else if (inString == (readCurrent + "\n"))    { Serial.print("Current (A) : ");  Serial.print(read_i2c(Reg_Read_Current),DEC);}
  else if (inString == (help + "\n"))           { printCommands()             ;}
  else if (inString == (stat + "\n"))           { showStat()                  ;}
  else if (inString == (logstart + "\n"))       { StartLog()                  ;}

  //if string contains setRPM command, parse out the value and set rpm to that value.
  else if(inString.indexOf(setRPM) >=0)
  {
    // The data to be parsed
    char *p = inData;
    // Temp store for each data chunk
    char *str;   
    // Id ref for each chunk 
    int count = 0;
      
    // Loop through the data and seperate it into
    // chunks at each "," delimeter
    while ((str = strtok_r(p, "=", &p)) != NULL)
    { 
      // Add chunk to array  
      inParse[count] = str;
      // Increment data count
      count++;      
    }
    
    // If the data has two values then..  
    if(count == 2)
    {
      // Define value 1 as a function identifier
      char *func = inParse[0];
      // Define value 2 as a property value
      char *prop = inParse[1];
      String A = prop;
      String B = func;
      int rpm_level = A.toInt();

      if (B = "rpm")
      {
        // Set_RPM(prop)
        write_i2c(Reg_Set_RPM_Addr,rpm_level);
      }
    }  
  }

}

bool write_i2c(byte reg, word val)
{
  // convert value to high and low bytes
   unsigned char valHigh = (unsigned char)((val >> 0x08) & 0xff);
   unsigned char valLow  = (unsigned char)(val & 0xff);
  
   Wire.beginTransmission(Motor_i2c_address);    // begin transmit to Motor
   Wire.write(reg);
   Wire.write(valHigh);
   Wire.write(valLow);
   error = Wire.endTransmission();               // stop transmitting
   return error;
}

word read_i2c(byte reg)
{
  char y= 0;
  Wire.beginTransmission(Motor_i2c_address);    // begin transmit to Motor
  Wire.write(reg);
  error = Wire.endTransmission();               // stop transmitting

  Wire.requestFrom(Motor_i2c_address, 2);
  while (Wire.available()) 
  {
    incomingByte[y] = Wire.read();                       // receive a byte as character
    y++;
  }
  y=0;
  int full_16_bit = (incomingByte[0] *256) + incomingByte[1];
  return (word)full_16_bit;
}

void En_Motor(bool EN, bool dir) 
{
  char i = 0;
  
  if ( EN + dir == 0)
  {
    
   Serial.print("Disabling Motor");
   write_i2c(Reg_Sys_Config_Addr,0x0000);
   Serial.print("........");
   Serial.print("DONE\n");
  
  }
  
  else if (dir == Rotate_CW)
  {
   Serial.print("Enabling Motor CW");
  
   write_i2c(Reg_Sys_Config_Addr,0x0001);

   for (i=0; i<WAIT_TIME; i++)
   {
     Serial.print(".");
     delay(1000);
   }
   Serial.print("DONE\n");
   
  }   
  
  else if (dir == Rotate_CCW)
  {
   
   Serial.print("Enabling Motor CCW");

   write_i2c(Reg_Sys_Config_Addr,0x0003);
   for (i=0; i<WAIT_TIME; i++)
   {
     Serial.print(".");
     delay(1000);
   }
   Serial.print("DONE\n");
   
  }
}

void printCommands()
{
  Serial.println("Commands:");
  Serial.println(help           + "\t -Display commands ");
  Serial.println(stat           + "\t -Display Motor Status");
  Serial.println(readFW         + "\t -Display FW version  ");
  Serial.println(readTemp       + "\t -Display Temp  ");
  Serial.println(readRPM        + "\t -Display RPM  " );
  Serial.println(readCurrent    + "\t -Display Current  ");
  Serial.println(enable_motor   + "\t -Enable Motor  ");
  Serial.println(disable_motor  + "\t -Disable Motor  ");
  Serial.println(setRPM + "<value up to 9000>" + "\t -Set RPM  ");
}

void showStat()
{
  Serial.println("Status:");
  Serial.print("Temp (C)\t: ");     
  Serial.println(read_i2c(Reg_Read_Temp),DEC);
  delay(20);
  Serial.print("RPM\t\t: ");         
  Serial.println(read_i2c(Reg_Read_RPM_Addr),DEC);
  delay(20);
  Serial.print("Current (mA)\t: ");  
  Serial.println(read_i2c(Reg_Read_Current),DEC);
  delay(20);
}


