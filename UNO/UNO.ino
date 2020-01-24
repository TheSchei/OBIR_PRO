#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

//well-known/core string
#define CORE "</frequency>;rt=\"frequency\";ct=0,</potencjometr>;rt=\"potencjometr\";ct=0,</udprecs>;rt=\"udprecs\";ct=0,</errors>;rt=\"errors\";ct=0,</ministats>;rt=\"ministats\";ct=0,</debug>;rt=\"debug\";ct=0"

byte mac[]={0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf6};
EthernetUDP Udp;
short localPort = 2356;
char packetBuffer[32];
boolean ifCON=true; // checks if it is a CON/NON message
RF24 radio(7, 8);
RF24Network network(radio);

//struct for communication with MiniPro
struct payload_t  
{
  char type = 0;//type od message
  int value;//value
};

//CoAP Message ID
union MID
{
  char x[2];
  unsigned int MID;
};

struct payload_t payload;
RF24NetworkHeader headerRec(1), headerSend(1);

//Statystyki
unsigned long ReceivedUDPMessages = 0;
unsigned long ErrorCounter = 0;
unsigned long temp;//TEMP

//Zmienne główne
MID mid; // message id
unsigned char TKL; // token length
char Token[15]; //CoAP token
byte UriPath; //UriPath encoded on one byte
unsigned int option_accept; //number of option Accept
unsigned int option_content;//number of option content format
char code; // type of message (GET, PUT, etc)
byte errorFlag = 0;//flag of error

//zmienne pomocnicze
boolean flag;
boolean flagForDebug=false;
unsigned int option;
unsigned int option_delta;
unsigned int option_length;

void setup() {
  SPI.begin();
  radio.begin();
  network.begin(67,0);
  Ethernet.begin(mac);
  Udp.begin(localPort);
  Serial.begin(115200);
  Serial.println(F("Witamy w naszym gitara siema"));
  Serial.println(Ethernet.localIP());
}

void loop() {
  network.update();
  if (errorFlag) //Error occured in previous iteration
  {
    if(ifCON)
      packetBuffer[0] = 96; // v1 ACK
    else
      packetBuffer[0] = 80; // v1 NON
    //CODE
    switch(errorFlag)
    {
      case 255: //UNKNOWN PROTOCOL
        packetBuffer[0] = 96; //v1 ACK TKL = 0
        packetBuffer[1] = 0b10000000; //CODE 4.00 BAD REQUEST
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0]; //Randomowy syf bedzie?
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Bad Request", strlen("Bad Request"));
        Udp.endPacket();
        break;

      case 1: //UNKNOWN TYPE FIELD
        packetBuffer[0] += TKL;
        packetBuffer[1] = 0b10000000; //CODE 4.00 BAD REQUEST
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0];
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        Udp.write(Token, TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Bad Request", strlen("Bad Request"));
        Udp.endPacket();
        break;

      case 128: //UNKNOWN URI PATH
        packetBuffer[0] += TKL;
        packetBuffer[1] = 0b10000100; //CODE 4.04 NOT FOUND
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0];
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        Udp.write(Token, TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Not Found", strlen("Not Found"));
        Udp.endPacket();
        break;

      case 129: //UNKNOWN CONTENT-FORMAT
        packetBuffer[0] += TKL;
        packetBuffer[1] = 0b10001111; //CODE 4.15 UNSUPPORTED CONTENT-FORMAT
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0];
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        Udp.write(Token, TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Unsupported Content-Format", strlen("Unsupported Content-Format"));
        Udp.endPacket();
        break;

      case 130: //UNKNOWN ACCEPT FORMAT
        packetBuffer[0] += TKL;
        packetBuffer[1] = 0b10000110; //CODE 4.06 NOT ACCEPTABLE
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0];
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        Udp.write(Token, TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Not Acceptable", strlen("Not Acceptable"));
        Udp.endPacket();
        break;

      case 131: //UNKNOWN OPTION
        packetBuffer[0] += TKL;
        packetBuffer[1] = 0b10000010; //CODE 4.02 BAD OPTION
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0];
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        Udp.write(Token, TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Bad Option", strlen("Bad Option"));
        Udp.endPacket();
        break;

      default:
        packetBuffer[0] += TKL;
        packetBuffer[1] = 0b10100000; //CODE 5.00 INTERNAL SERVER ERROR
        packetBuffer[2] = mid.x[1];
        packetBuffer[3] = mid.x[0];
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(packetBuffer, 4);
        Udp.write(Token, TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 0;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write("Internal Server Error", strlen("Internal Server Error"));
        Udp.endPacket();
        break;
    }

    ErrorCounter++;
    errorFlag = 0;
    return;
  }
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    ReceivedUDPMessages++;
    Serial.print(F("Processing package, size = "));
    Serial.println(packetSize);
    memset(packetBuffer, 0 ,sizeof(packetBuffer));
    int r = 0;
    if (packetSize < 4) //ERROR (unknown protocol)
    {
      errorFlag = 255;
      return;
    }
    else // First block read
    {
      Udp.read(packetBuffer, 4); //Read first 4 bytes of message
      if((packetBuffer[0] & 0b11110000) == 64)//v1 CON message
      {
        TKL = (unsigned char)(packetBuffer[0] & 0b00001111); //Read Token length (second 4 bits of first byte)
        mid.x[1] = packetBuffer[2]; mid.x[0] = packetBuffer[3]; //Read MessageID (third and fourth byte)
        ifCON=true;
        if((code=packetBuffer[1]) == 0)//PING empty message
        {
          packetBuffer[0] = 96;//ACK, v1, TKL == 0
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
          Udp.write(packetBuffer, 4); //Send response (the same code and message id)
          Udp.endPacket();
          return;
        }
      }
      else if((packetBuffer[0] & 0b11110000) == 80) // NON message
      {
        TKL = (unsigned char)(packetBuffer[0] & 0b00001111); //Read Token length (second 4 bits of first byte)
        code=packetBuffer[1];
        mid.x[1] = packetBuffer[2]; mid.x[0] = packetBuffer[3]; //Read MessageID (third and fourth byte)
        ifCON=false;
      }
      else if((packetBuffer[0] & 0b11110000) == 96) return; //ACK message
      else // ERROR, unknown type
      {
        errorFlag = 1;
        return;
      }
      
      
      Serial.println(F("Reading Token"));
      
      Udp.read(packetBuffer, TKL); //Read TKL bytes of message  //sprawdzić, czy działą poprawnie przy TKL==0
      for (int i=0; i<TKL; i++)
        Token[i] = packetBuffer[i]; // Save Token in Token variable

      Serial.println(F("Reading options"));
      
      //setting init values
      
      option = 0;
      UriPath = 0;
      option_accept = 0;
      option_content = 0;
      int r=0;
      while((((r=Udp.read(packetBuffer, 1))!=-1) && (packetBuffer[0] != -1)))
      {
        option_delta = ((unsigned int)((packetBuffer[0] & 0b11110000) >> 4));//read option_delta
        option_length = (unsigned int)(packetBuffer[0] & 0b00001111);//read option length
        if (option_delta == 13)//check if there is more bytes to read
        {
          Udp.read(packetBuffer, 1);//read rest of the option
          option_delta += ((unsigned int)(packetBuffer[0]));
        }
        else if (option_delta > 13)//error, unhandled option
        {
          errorFlag = 131;
          break;
        }
        if (option_length == 13)//check if there is more bytes to read
        {
          Udp.read(packetBuffer, 1);//read rest of the option
          option_length += ((unsigned int)(packetBuffer[0]));
        }
        else if (option_length > 13)//error, unhandled option
        {
          errorFlag = 131;
          break;
        }
        option += option_delta;//setting option number
        Serial.print(F("Option delta: "));
        Serial.println(option_delta);
        Serial.print(F("Option length: "));
        Serial.println(option_length);
        Serial.print(F("Option number: "));
        Serial.println(option);
        
        if (option == 11)// URI-Path
          if (option_length > 17) errorFlag = 128; //error
          else 
          {
            memset(packetBuffer,'\0' ,sizeof(packetBuffer));//clear buffer
            Udp.read(packetBuffer, option_length);//read value
            if (!strcmp(packetBuffer, ".well-known") && UriPath == 0) UriPath = 128;  //Checking URI-Path
            else if (!strcmp(packetBuffer, "core") && UriPath == 128) UriPath = 192;
            else if (!strcmp(packetBuffer, "frequency") && UriPath == 0) UriPath = 16;
            else if(!strcmp(packetBuffer, "potencjometr") && UriPath==0) UriPath=8;
            else if(!strcmp(packetBuffer, "udprecs") && UriPath==0) UriPath=4;
            else if(!strcmp(packetBuffer, "errors") && UriPath==0) UriPath=2;
            else if(!strcmp(packetBuffer, "debug") && UriPath==0) UriPath=3;
            else if(!strcmp(packetBuffer, "ministats") && UriPath==0) UriPath=1;
            else errorFlag = 128;//UNKNOWN URI_PATH
            Serial.print(F("UriPath = "));
            Serial.println(UriPath);
          }
        else if (option == 12) //Content_Format
        {
          if (option_length == 1)
          {
            Udp.read(packetBuffer, option_length);
            option_content = (unsigned int)packetBuffer[0];
          }
          else errorFlag = 129;//UNKNOWN FORMAT (>1 byte, so >128, we use max 50)
          if (option_content != 0)// && option_content != 50)// we can read only plaintext(put frequency)
            errorFlag = 129;//UNKNOWN FORMAT
        }
        else if (option == 17)//Accept
        {
          if (option_length == 1)
          {
            Udp.read(packetBuffer, option_length);
            option_accept = (unsigned int)packetBuffer[0];
          }
          else errorFlag = 130;//UNKNOWN ACCEPT (>1 byte, so >128, we use max 50)
          if ((option_accept != 0 && option_accept != 50) || (UriPath == 192 && option_accept == 40))//application/linkformat case
            errorFlag = 130;//UNKNOWN ACCEPT
        }
        else errorFlag = 131;//UNKNOWN OPTION
      }
      Serial.println(F("End of while"));
      if(errorFlag) return;
      
      Udp.read(packetBuffer, sizeof(packetBuffer));//zakładamy, że payload nie może przekraczać wielkości bufora
      
      if(UriPath == 192 && code == 1)// /.well-known/core
      {
        Serial.println(F("Send response"));
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;//ACK + TKL
        packetBuffer[1] = 0b01000101;//2.05
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0];
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL);
        packetBuffer[0] = 0b11000001;//content format
        packetBuffer[1] = 40;//application/link format
        packetBuffer[2] = 0b11111111;//payload marker
        Udp.write(packetBuffer, 3);
        Udp.write(CORE, sizeof(CORE));
        Udp.endPacket();
      }
      else if(((UriPath == 16) || (UriPath == 1)) && (code == 1))// GET /frequency or GET/miniStats
      {
        if(UriPath == 16) payload.type = 2;//GET frequency
        else payload.type = 4;//GET miniStats
        payload.value = 0;
        network.write(headerSend, &payload, sizeof(payload));
        while(!network.available()) network.update(); // communication with mini pro
        network.read(headerRec, &payload, sizeof(payload));  
         Udp.beginPacket(Udp.remoteIP(), Udp.remotePort()); // we got value and we'll send it to client
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000101; //2.05
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0]; 
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL); //the same token
        packetBuffer[0] = 0b11000001;
        packetBuffer[2] = 0b11111111; // payload marker
        if (option_accept == 0) 
        {
          packetBuffer[1] = 0;
          Udp.write(packetBuffer, 3);
          memset(packetBuffer,'\0' ,strlen(packetBuffer));
          itoa(payload.value, packetBuffer, 10);
        }
        else if (option_accept == 50)
        {
          packetBuffer[1] = 50;
          Udp.write(packetBuffer, 3);
          sprintf(packetBuffer, "{\"value\": %d}", payload.value);
        }
        Udp.write(packetBuffer, strlen(packetBuffer));
        Udp.endPacket();
      }
      else if((UriPath == 16) && (code == 3))// PUT /frequency
      {
        payload.type = 1;//PUT frequency 
        if (option_content == 0)
        {
          payload.value = atoi(packetBuffer);
        }
        else return;//JSONA JESZCZE
        network.write(headerSend, &payload, sizeof(payload));//moze jeszcze jakies potwierdzenie
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000100; //2.04
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0]; 
        Udp.write(packetBuffer, 4);
        if(TKL > 0) Udp.write(Token, (int)TKL); //the same token
        Udp.endPacket();
      }
      else if((UriPath==8) && (code==1)) // GET potencjometr
      {
        if(ifCON) // if it is a CON message, we should send ACK to client
        {
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort()); //wysylamy ack do klients
          packetBuffer[0]=0b01100000; // ACK
          packetBuffer[1]=0b00000000; //empty token
          packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0]; // the same message id
          Udp.write(packetBuffer, 4);
          Udp.endPacket();
        }
        payload.type=3;
        payload.value=0;
         network.write(headerSend, &payload, sizeof(payload)); // wysyłamy do mini pro tego geta
        while(!network.available()) network.update(); 
        network.read(headerRec, &payload, sizeof(payload)); //dostajemy

        boolean flag=false;
        while(true)//zamieniłbym to na MAX 3 razy
        //for (int i=0; i<3; i++)
        {
        temp=millis();
        Serial.println(F("Transmision to client"));
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 64 + (char)TKL;
        packetBuffer[1] = 0b01000101; //2.05
        packetBuffer[2] = mid.x[1]+1; packetBuffer[3] = mid.x[0];//different message id 
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL); //the same token
        
        packetBuffer[0] = 0b11000001;
        packetBuffer[2] = 0b11111111; // payload marker
        if (option_accept == 0) 
        {
          packetBuffer[1] = 0;
          Udp.write(packetBuffer, 3);
          memset(packetBuffer,'\0' ,sizeof(packetBuffer));
          itoa(payload.value, packetBuffer, 10);
        }
        else if (option_accept == 50)
        {
          packetBuffer[1] = 50;
          Udp.write(packetBuffer, 3);
          sprintf(packetBuffer, "{\"value\": %d}", payload.value);
        }
        Udp.write(packetBuffer, strlen(packetBuffer));
        Udp.endPacket(); // odsyłamy odczytaną wartość do klienta który żądał
        if(flagForDebug)
        { 
          Serial.println(F("Pokazujemy retransmisje"));
          while((millis()-temp)<=5000)
          {
          }
          flagForDebug=false;
        }
        while((millis()-temp)<=3000)
        {
          
          //Serial.println(F("millis"));
          packetSize=Udp.parsePacket();
          if (packetSize)
          {
            Udp.read(packetBuffer,4);
            if((packetBuffer[2]==mid.x[1]+1) && (packetBuffer[3]==mid.x[0]) && ((packetBuffer[0] & 0b11110000) == 96)) // jeśli to ACK z tym samym MID to koniec
            {
             Serial.println("received ACK");
             flag=true;
             break;
            }
          }
         }
         if(flag) break; 
         else Serial.println(F("Retransmisja"));
        }      
      }
      else if(((UriPath==4) || (UriPath==2)) && (code==1))// /udprecs || /errors
      {
        if (UriPath==4) temp = ReceivedUDPMessages;
        else if (UriPath==2) temp = ErrorCounter;
        //else if (UriPath==1) temp = XXXXX;
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000101;
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0];
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[2] = 0b11111111;
        if (option_accept == 0) 
        {
          packetBuffer[1] = 0;
          Udp.write(packetBuffer, 3);
          memset(packetBuffer,'\0' ,sizeof(packetBuffer));
          itoa(temp, packetBuffer, 10);
        }
        else if (option_accept == 50)
        {
          packetBuffer[1] = 50;
          Udp.write(packetBuffer, 3);
          sprintf(packetBuffer, "{\"value\": %d}", temp);
        }
        Udp.write(packetBuffer, strlen(packetBuffer));
        Udp.endPacket();
      }
      else if((UriPath==3) && (code==1))
      {
        Serial.println(F("Debug online"));
        flagForDebug=true;
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000101;
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0];
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[2] = 0b11111111;
        if (option_accept == 0) 
        {
          packetBuffer[1] = 0;
          Udp.write(packetBuffer, 3);
          strcpy(packetBuffer, "debug");
        }
        else if (option_accept == 50)
        {
          packetBuffer[1] = 50;
          Udp.write(packetBuffer, 3);
          strcpy(packetBuffer, "{\"debug\": true}");
        }
        Udp.write(packetBuffer, strlen(packetBuffer));
        Udp.endPacket();
      }
      else errorFlag = 128;
     }
    }   
 }
