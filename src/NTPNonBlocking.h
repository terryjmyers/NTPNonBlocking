/*
 Name:		NTPNonBlocking.h
 Created:	1/7/2020 8:15:22 AM
 Author:	terry
 Editor:	http://www.visualmicro.com
 www.GitHub.com/TerryJMyers/NTPNonBlocking







 Example (kind of pseudocode...i.e. I didn't test this)
	 #include <NTPNonBlocking.h>
	 NTPNonBlocking NTP;
	 void setup(void) {
		//Setup Serial and wifi or whatever
		NTP.init()
	 }
	  void loop(void) {
		if (NTP.Handle() == false) Serial.println(NTP.ErrorMessage);
		Serial.println(NTP.UnixTime);
		delay(100);
	 }



Bonus - My implementation of an internet Check function that I use as a pointer for _isInternetFunc
	#include <ESP8266Ping.h>

	uint32_t CheckForInternetTimer;
	uint32_t PingFailureCounter;
		bool isInternet(bool force = false) {
	
			if (bitRead(WiFi.getMode(), 0) && WiFi.status() == WL_CONNECTED && !WiFiConnecting) { //WiFiConnecting is used in my programs as I've implemented non blocking wifi connections, you should delete this
				if ((millis() - CheckForInternetTimer > 29989) || force) {
					if (Ping.ping({ 8,8,8,8 }, 1)) {
						CheckForInternetTimer = millis();
						PingFailureCounter = 0;
					}
					else {//ping failed
						PingFailureCounter++;
						CheckForInternetTimer = millis() - 29989 + 8447; //Check again about 8 seconds later
					}
				}
			}
			else {//WiFi is not the correct mode or is not connected
				PingFailureCounter = 99;
			}
		return (PingFailureCounter <=3 );

		}


*/

#ifndef _NTPNonBlocking_h
#define _NTPNonBlocking_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

class NTPNonBlocking
{
public:
	NTPNonBlocking();//Constructor

	/*
	init
		Run this in Setup()
	PARAMS:
		__isInternetFunc (Optional)
			Pointer to a function that returns if the device has internet access. Default is none and assume it always has internet.
			Library does check for WiFi.Status() ==WL_CONNECTED, so in most cases it should be find to leave out
		__SyncIntervalTime (Optional)
			How often to sync to NTP. Default is every 23 hours
		__NTPServerName
			Obvious...
	RETURN
		none
	*/
	void init(bool (*__isInternetFunc)(bool) = NULL, uint32_t __SyncIntervalTime = 82800000, String __NTPServerName = "pool.ntp.org"); // Default Sync Interval time 23h(prime number) or 82, 800, 000ms

	/*
	Handle
		Call This every loop.  Internally it only processes every HandleTime amount of ms. to reduce CPU load
	PARAMS:
		none
	RETURN
		true =	no error
		false =	error
			false is only returns once (a one shot).  The next Handle will return true.  Use this as a trigger to Serial.println(NTP.ErrorMessage) if desired
			i.e. when false, ErrorMessage will be non-blank, Serial.println() this String immediatly
	*/
	bool Handle(void);		//Call this in Loop every scan

	//Human Readable ErrorMessage set to non blank when Handle() returns false.  Cleared when Handle() successfully syncs to NTP
	String ErrorMessage;	//Always blank except when Handle returns false (error).  Immediatly print message (if desired).
							//Cleared on a successful NTP sync, so print this message (if desired) whenever Handle returns false
							//Handle will return false only once (i.e. the next loop it will return true), but the ErrorMessage still remains until the next successful NTP sync

	//The whole point of this function...to return a Unix Time.  Valid if > 0.
	uint32_t UnixTime;		

	//Set to true on a one shot.  Handle will reset back to false.  Only works if library is waiting on sync interval
	bool ForceTimeSyncFlag = false;		

	//Set during init method call, but it may be useful for you to set this any time
	String				NTPServerName; 

	/*
	SyncIntervalTime
	Time in ms that Handle will resync with an NTP Server
	Default is 82800000 (23hours)
	Set during init method call, but it may be useful for you to set this any time 
	*/
	uint32_t			SyncIntervalTime;

	//Time in ms to actually do something in Handle() method.  Used to prevent Handle() from doing uncessesary work every loop.  
	//Default is fine.  But if you want to change it choose a prime number under 1000ms (i.e. 997 or under)
	uint32_t			HandleTime = 11; 

private:

	bool				_SendNTPPacket(void);		//Send NTP Packet Function
	uint32_t			_CheckForNTPResponse(void); //Checks for NTP Response

	bool				_NoWiFiONS;					//One shot remember bit for when there is no wifi or internet to turn stuff off on a one shot

	//Timer to prevent processing of library every loop.  Used in conjunction with HandleTime
	uint32_t			_HandleTimer;

	//Stuff to handle Deterministic State Engine used to implent non blocking functionality.  See .c file for more information
	int8_t				_Step;
	int8_t				_StepCMD;
	int8_t				_StepREM = -1;
	uint32_t			_NTPResponseTimeoutTimer;
	uint32_t			_RetryTimer;
	uint32_t			_RetryTime;

	//NTP
	bool				(*_isInternetFunc)(bool) = NULL;	//pointer to a function that returns if there is internet 
															
	WiFiUDP				_udp;						// A UDP instance to let us send and receive packets over UDP
	const unsigned int	_localPort = 2390;			// local port to listen for UDP packets	
	byte				_packetBuffer[48];			//buffer to hold incoming and outgoing packets

	uint32_t			_SyncIntervalTimer;		

	uint32_t			_UnixTimeLastUpdate;		//The Unix time given by server at the iync interval
	uint32_t			_UnixTimeLastUpdateSecond;	//_Seconds recorded at the time of the last update interval.  The difference between _Seconds and this is the time elapsed

	//Internal Second Incrementor.
	uint32_t			_Seconds;					//Seconds since last init() method call
	uint32_t			_OneSecondTimer;			//millis remember for _Seconds timer

};


#endif

