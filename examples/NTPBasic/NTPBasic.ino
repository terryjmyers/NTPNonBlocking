/*
 Name:		NTPBasic.ino
 Created:	1/8/2020 7:15:33 PM
 Author:	terry
*/



#include <ESP8266WiFi.h>
#include <NTPNonBlocking.h>
NTPNonBlocking NTP;


void setup(void) {
	
	Serial.begin(115200);
	WiFi.begin("205", "xskdajss");//initiate the connection
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
		if (millis() > 15000) {
			Serial.println("WiFi Failed to connect");
			delay(1000);
			yield();
		}
	}

	NTP.init();
}
void loop(void) {
	if (NTP.Handle() == false) Serial.println(NTP.ErrorMessage);
	Serial.println(NTP.UnixTime);
	delay(100);
}
