#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#define CORE "</test>;rt=\"test\";ct=0,</frequency>;rt=\"frequency\";ct=0,</potencjometr>;rt=\"potencjometr\";ct=0,</udprecs>;rt=\"udprecs\";ct=0,</errors>;rt=\"errors\";ct=0,</ministats>;rt=\"ministats\";ct=0,</debug>;rt=\"debug\";ct=0"



byte mac[]={0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf6};
EthernetUDP Udp;
short localPort = 2356;
char packetBuffer[32];
boolean ifCON=true; // checks if it is a CON/NON message
RF24 radio(7, 8);
RF24Network network(radio);

struct payload_t  
{
  char type = 0;
  int value;
};

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
unsigned char TKL; // długosc tokena
char Token[15];
byte UriPath;
unsigned int option_accept;
unsigned int option_content;
char code;
byte errorFlag = 0;

//zmienne pomocnicze
boolean flag;
boolean flagForDebug;
byte option;
byte option_delta;
byte option_length;

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
  if (errorFlag)
  {
    //CODE
    ErrorCounter++;
    errorFlag = 0;
    return;
  }
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    ReceivedUDPMessages++;
    Serial.println(F("Mamy pakiet"));
    Serial.println(packetSize);
    memset(packetBuffer, 0 ,sizeof(packetBuffer));
    int r = 0;
//    Udp.read(packetBuffer, sizeof(packetBuffer));
    
    if (packetSize < 4) //ERROR (unknown protocol)
    {
      errorFlag = 255;
      return;
    }
    else
    {
//      for(int i=0;i<packetSize;i++)
//      {
//        Serial.print(packetBuffer[i]);
//      }
//      Serial.println();
      Udp.read(packetBuffer, 4);
      if((packetBuffer[0] & 0b11110000) == 64)//CON message
      {
        TKL = (unsigned char)(packetBuffer[0] & 0b00001111);
        mid.x[1] = packetBuffer[2]; mid.x[0] = packetBuffer[3];
        ifCON=true;
        if((code=packetBuffer[1]) == 0)//PING
        {
          packetBuffer[0] = 96;//ACK, v1, TKL == 0
          Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
          Udp.write(packetBuffer, 4);
          Udp.endPacket();
          return;
        }
      }
      else if(packetBuffer[0] & 0b11110000 == 80)
      {
          ifCON=false;//NON
      }
      else if(packetBuffer[0] & 0b11110000 == 96) return;//ACK
      else//ERROR
      {
        errorFlag = 1;
        return;
      }
      
      
      Serial.println(F("Reading Token"));
      
      Udp.read(packetBuffer, TKL);//sprawdzić, czy działą poprawnie przy TKL==0
      for (int i=0; i<TKL; i++)
        Token[i] = packetBuffer[i];

      Serial.println(F("Reading options"));
      
      option = 0;
      UriPath = 0;
      option_accept = 0;
      option_content = 0;
      int r=0;
      while(((r=Udp.read(packetBuffer, 1))!=-1) && (packetBuffer[0] != -1))
      {
        Serial.print(F("Read bytes: "));
        Serial.println(r);
        //delay(5000);
        option_delta = ((byte)((packetBuffer[0] & 0b11110000) >> 4));
        Serial.print(F("Option delta: "));
        Serial.println(option_delta);
        option_length = (byte)(packetBuffer[0] & 0b00001111);
         Serial.print(F("Option length: "));
        Serial.println(option_length);
        if (option_delta == 13)
        {
          Udp.read(packetBuffer, 1);
          option_delta += ((byte)(packetBuffer[0]));
        }
        if (option_length == 13)
        {
          Udp.read(packetBuffer, 1);
          option_length += ((byte)(packetBuffer[0]));
        }
        option += option_delta;
        //Serial.print(F("Option number: "));
        Serial.println(option);
        
        if (option == 11)// URI-Path
          if (option_length > 17) flag = false; //error
          else 
          {
            memset(packetBuffer,'\0' ,sizeof(packetBuffer));
            Udp.read(packetBuffer, option_length);
            Serial.println(packetBuffer);
            if (!strcmp(packetBuffer, ".well-known") && UriPath == 0) UriPath = 128;
            else if (!strcmp(packetBuffer, "core") && UriPath == 128) UriPath = 192;
            else if (!strcmp(packetBuffer, "test") && UriPath == 0) UriPath = 32;
            else if (!strcmp(packetBuffer, "frequency") && UriPath == 0) UriPath = 16;
            else if(!strcmp(packetBuffer, "potencjometr") && UriPath==0) UriPath=8;
            else if(!strcmp(packetBuffer, "udprecs") && UriPath==0) UriPath=4;
            else if(!strcmp(packetBuffer, "errors") && UriPath==0) UriPath=2;
            else if(!strcmp(packetBuffer, "ministats") && UriPath==0) UriPath=1;
            else if(!strcmp(packetBuffer, "debug") && UriPath==0)
            {
              Serial.println(F("Changed flag"));
              flagForDebug=true;
              UriPath=3;
            }
            else 
            {
              flag = false;//nie wiem po co
              errorFlag = 128;//UNKNOWN URI_PATH
            }
            Serial.print(F("UriPath = "));
            Serial.println(UriPath);
          }
        else if (option == 12) //Content_Format
        {
          Udp.read(packetBuffer, option_length);
          for (int i = 0; i< option_length; i++)
          {
            option_content <<= 8;
            option_content += (unsigned int)packetBuffer[i];
          }
          if (option_content != 0 && option_content != 50)
            errorFlag = 129;//UNKNOWN FORMAT
        }
        else if (option == 17)//Accept
        {
          Udp.read(packetBuffer, option_length);
          for (int i = 0; i< option_length; i++)
          {
            option_accept <<= 8;
            option_accept += (unsigned int)packetBuffer[i];
          }
          if (option_accept != 0 && option_accept != 50)
            errorFlag = 130;//UNKNOWN ACCEPT
        }
        else 
        {
          flag = false;//error
          errorFlag = 131;//UNKNOWN OPTION
        }
      }
      Serial.println(F("End of while"));
      if(errorFlag) return;
      //brutalna optymalizacja
      Udp.read(packetBuffer, sizeof(packetBuffer));//zakładamy, że payload nie może przekraczać wielkości bufora
      
      if(UriPath == 192)// /.well-known/core // jeszcze powinien być warunek code == 1, czyli GET
      {
        Serial.println(F("Send response"));
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;
        //packetBuffer[1] = 69;
        packetBuffer[1] = 0b01000101;
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0];
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL);
        packetBuffer[0] = 0b11000001;
        packetBuffer[1] = 40;
        packetBuffer[2] = 0b11111111;
        Udp.write(packetBuffer, 3);
        Udp.write(CORE, sizeof(CORE));
        Udp.endPacket();
      }
      else if(UriPath == 32)// /test // jeszcze powinien być warunek code == 1, czyli GET
      {
        Serial.println(F("Send response test"));
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000101;
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0];
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL);
        packetBuffer[0] = 0b11000001;
        //packetBuffer[1] = 0; //packetBuffer[1] = option_accept możnaby tak zrobić
        //DODALEM SPRAWDZANIE OPCJI, RACZEJ MOZNA ODKOMENTOWAC
        packetBuffer[2] = 0b11111111;
        //Udp.write(packetBuffer, 3);
        if (option_accept == 0) 
        {
          packetBuffer[1] = 0;
          Udp.write(packetBuffer, 3);
          strcpy(packetBuffer, "plaintext, or anything");
        }
        else if (option_accept == 50)
        {
          packetBuffer[1] = 50;
          Udp.write(packetBuffer, 3);
          strcpy(packetBuffer, "{text: \"plaintext, or anything\"}");
        }
        Udp.write(packetBuffer, sizeof(packetBuffer));
        Udp.endPacket();
      }
      else if((UriPath == 16) && (code == 1))// GET /frequency
      {
        payload.type = 2;//GET frequency 
        payload.value = 0;
        network.write(headerSend, &payload, sizeof(payload));
        while(!network.available()) network.update(); // communication with mini pro
        network.read(headerRec, &payload, sizeof(payload));  
         Udp.beginPacket(Udp.remoteIP(), Udp.remotePort()); // we got frequency and we'll send it to client
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000101; //2.05
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0]; 
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL); //the same token
        packetBuffer[0] = 0b11000001;
        //packetBuffer[1] = 0; //packetBuffer[1] = option_accept możnaby tak zrobić, ale trzeba by zostawić to gotowe na zbugowanie, albo zabezpieczyć przed content-formatem o id > 255
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
          sprintf(packetBuffer, "{value: %d}", payload.value);
        }
        Udp.write(packetBuffer, sizeof(packetBuffer));
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
        while(true)
        {
        int x=millis();
        Serial.println(F("Transmision to client"));
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
         packetBuffer[0] = 64 + (char)TKL;
        packetBuffer[1] = 0b01000101; //2.05
        packetBuffer[2] = mid.x[1]+1; packetBuffer[3] = mid.x[0];//different message id 
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL); //the same token
        
        packetBuffer[0] = 0b11000001;
        //packetBuffer[1] = 0; //packetBuffer[1] = option_accept możnaby tak zrobić, ale trzeba by zostawić to gotowe na zbugowanie, albo zabezpieczyć przed content-formatem o id > 255
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
          sprintf(packetBuffer, "{value: %d}", payload.value);
        }
        Udp.write(packetBuffer, sizeof(packetBuffer));
        Udp.endPacket(); // odsyłamy odczytaną wartość do klienta który żądał
        if(flagForDebug)
        { 
          Serial.println(F("Pokazujemy retransmisje"));
          while(millis()-x<=5000)
          {
          }
          flagForDebug=false;
        }
        while(millis()-x<=3000)
        {
          
          Serial.println(F("millis"));
          packetSize=Udp.parsePacket();
          if (packetSize)
          {
            Udp.read(packetBuffer,4);
            if((packetBuffer[2]==mid.x[1]+1) && (packetBuffer[3]==mid.x[0]) && ((packetBuffer[0] & 0b11110000) == 96)) // jeśli to ACK z tym samym MID to koniec
            {
             Serial.println("jest git");
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
        //packetBuffer[1] = 0; //packetBuffer[1] = option_accept możnaby tak zrobić, ale trzeba by zostawić to gotowe na zbugowanie, albo zabezpieczyć przed content-formatem o id > 255
        packetBuffer[2] = 0b11111111;
        //Udp.write(packetBuffer, 3);
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
          sprintf(packetBuffer, "{value: %d}", temp);
        }
        Udp.write(packetBuffer, sizeof(packetBuffer));
        Udp.endPacket();
      }
      else if((UriPath == 1) && (code == 1)) // GET ministats
      {
        payload.type = 4;//GET stats 
        payload.value = 0;
        network.write(headerSend, &payload, sizeof(payload));
        while(!network.available()) network.update(); // communication with mini pro
        network.read(headerRec, &payload, sizeof(payload));  
         Udp.beginPacket(Udp.remoteIP(), Udp.remotePort()); // we got frequency and we'll send it to client
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
          memset(packetBuffer,'\0' ,sizeof(packetBuffer));
          itoa(payload.value, packetBuffer, 10);
        }
        else if (option_accept == 50)
        {
          packetBuffer[1] = 50;
          Udp.write(packetBuffer, 3);
          sprintf(packetBuffer, "{Messages received in MiniPro: %d}", payload.value);
        }
        Udp.write(packetBuffer, sizeof(packetBuffer));
        Udp.endPacket();
        
        
      }
      else if((UriPath==3) && (code==1))
      {
        Serial.println(F("Debug online"));
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        packetBuffer[0] = 96 + (char)TKL;
        packetBuffer[1] = 0b01000101;
        packetBuffer[2] = mid.x[1]; packetBuffer[3] = mid.x[0];
        Udp.write(packetBuffer, 4);
        Udp.write(Token, (int)TKL);
        packetBuffer[0] = 0b11000001;
        //packetBuffer[1] = 0; //packetBuffer[1] = option_accept możnaby tak zrobić
        //DODALEM SPRAWDZANIE OPCJI, RACZEJ MOZNA ODKOMENTOWAC
        packetBuffer[2] = 0b11111111;
        //Udp.write(packetBuffer, 3);
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
          strcpy(packetBuffer, "debug");
        }
        Udp.write(packetBuffer, sizeof(packetBuffer));
        Udp.endPacket();
      }
     }
    }   
 }
