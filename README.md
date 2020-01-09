# NTPNonBlocking
Non Blocking NTP Library 

Contacts NTP and provides a constantly updating Unix Time time that can be fed into a Time library as the time sync providor.  

Example:  
```
#include <ESP8266WiFi.h>  
#include <NTPNonBlocking.h>  

NTPNonBlocking NTP;  

void setup(void) {  
  
  //Connect to Serial and WiFi here....  
  
  NTP.init();    
}  
void loop(void) {  
  if (NTP.Handle() == false) Serial.println(NTP.ErrorMessage);  
  Serial.println(NTP.UnixTime);  
  delay(100);  
}  
```
