#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include "TimerOne.h"

RF24 radio(7, 8);
RF24Network network(radio);

struct payload_t  
{
  char type = 0;
  int value;
};

int value = 0;
int frequency = 11;
struct payload_t payload;
RF24NetworkHeader headerRec(0), headerSend(0);

void setup() {
 pinMode(10, OUTPUT);
 SPI.begin();
 radio.begin();
 network.begin(67,1);
 Serial.begin(115200);
 Timer1.initialize(500000);
 Timer1.attachInterrupt(myisr);
 Timer1.stop();
 Serial.println(F("Elo, mini"));
}

void myisr(){
  //digitalWrite(9,digitalRead(9)^1); // actual tone generation//niech mi nie halasuje :p
}

void loop() {
 //digitalWrite(10, digitalRead(10) ^ 1); // zmiana stanu membrany
 //i++;
 //delay(1);
 //if (i==1000)
 //{
 // i = 0;
 // Serial.println(F("freez"));
 // delay(2000);
 // }
  network.update();
  while(network.available())
  //if(network.available())
  {
    network.read(headerRec, &payload, sizeof(payload));
    switch(payload.type)
    {
    case 1:
        //set frequency
        if (payload.value <= 0) Timer1.stop();
        else if (payload.value < 1024)
        { 
          frequency = payload.value;
          Timer1.setPeriod(frequency<<9);//frequency*500//optymalizacja:p frequency<<9
          //Timer1.reset();
          Timer1.start();//ciekawe, czy jest na tyle mądre, że się nie wywala jeśli wystartujemy wystartowany
          Serial.print(frequency);
          Serial.println(F(" is a new frequency."));
        }
        else
        {
          Serial.print(payload.value);
          Serial.println(F("is too high frequency."));
        }
        break;
    case 2: // GET frequency
      
      payload.type = 1;//frequency :p
      payload.value = frequency;
      network.write(headerSend, &payload, sizeof(payload));
      Serial.println(F("Frequency sent to gateway."));
      break;
    case 3: //GET potencjometr
      value = 0;
      for(int i = 0; i < 16; i++) //likwidowanie wpływów szumów
      {
        value += analogRead(A0);
      }
      payload.type = 2;//potencjometr :p
      payload.value = value >>= 4;//ciekawe czy to działą :p
      network.write(headerSend, &payload, sizeof(payload));
      Serial.print(value);
      Serial.println(F(" value sent to gateway.."));
      break;
    default:
      Serial.println(F("Unknown type of message"));
      break;
    }
  }
}
