/*
 Name:		NTPNonBlocking.cpp
 Created:	1/7/2020 8:15:22 AM
 Author:	terry
 Editor:	http://www.visualmicro.com
*/

#include "NTPNonBlocking.h"

NTPNonBlocking::NTPNonBlocking(void)
{
}

void NTPNonBlocking::init(bool (*__isInternetFunc)(bool), uint32_t _SyncIntervalTime, String __NTPServerName) { //Default Sync Interval time 23h (prime number) or 82,800,000ms

	NTPServerName = __NTPServerName;	
	ErrorMessage.clear();
	UnixTime = 0;

	_isInternetFunc = __isInternetFunc;
	_HandleTimer = millis();
	_Step = 0;
	_StepCMD = 0;
	_StepREM = -1;
	_NoWiFiONS = false;
	   
	_SyncIntervalTimer = millis();
	if (_SyncIntervalTime < 120000) _SyncIntervalTime = 120000; //yeah...no...don't bother NTP so often...
	SyncIntervalTime = _SyncIntervalTime;

	_UnixTimeLastUpdate = 0;
	_UnixTimeLastUpdateSecond = 0;
	_OneSecondTimer = millis();
	_Seconds = 0;
}
/*
Use a deterministic state engine to handle sending NTP packs and waiting for responses in a Non-Blocking manner
Uses if...else if...else if...else if... in order to choose only one step
*/
bool NTPNonBlocking::Handle(void) {

	uint32_t now = millis();
	if (now - _HandleTimer >= HandleTime) { //only actually do something every so often, we don't need to process this EVERY loop...


		//Create a one timer that increments (on average) exactly once a second.  This is used as a basis of time instead of millis as it will last longer in a 32 bit uint
		if (now - _OneSecondTimer >= 1000) {
			_OneSecondTimer = _OneSecondTimer + 1000;
			_Seconds++;
			Serial.println(UnixTime);
		}

		bool _isInternet = true;
		if (_isInternetFunc != NULL) _isInternet = _isInternetFunc(false);

		//if (WiFi.status() == WL_CONNECTED && _isInternet(false)) {
		if (WiFi.status() == WL_CONNECTED && _isInternet) {
			
			_NoWiFiONS = false; //Set to false in case the above statement goes false to reset things only once
			if (ForceTimeSyncFlag && _Step == 40) _StepCMD = 100;
			if (_Step != _StepCMD) {
				_Step = _StepCMD;//Step sequencer control, if StepCMD is set, set Step
			}
			/*
			Sequencer:
				-------------------
				STEP00 - Initializes UDP(i.e. udp.begin()), this steps runs only once when WiFi is connected and internet is confirmed to begin udp
					SUCCESS - goto STEP10
					FAILURE - udp Fails to begin - goto STEP101 (Error Step)

				STEP10 - Start - Does nothing, bookend step for state engine completeness
					ALWAYS - goto STEP20

				STEP20 - Send NTP Packet
					SUCCESS - Start Response Timeout Timer - goto STEP30
					FAILURE - udp packet fails, dns fails, udp.write fails, etc - goto STEP101 (error step)

				STEP30 - Wait for NTP Response
					SUCCESS - Set Unix Time, Start SyncInterval Timer - goto STEP40
					FAILURE - Response Timeout timer expires - goto STEP101 (error step)

				STEP40 - Wait for next SyncInterval
					Timer Expires - goto STEP100

				STEP100 - Complete - Does nothing, bookend step for state engine completeness		
					ALWAYS - goto STEP10
				--------------------
				
				Error steps:
				STEP101 - Error Step - Start a Timer
					goto STEP102

				STEP102 - Wait for timer to expire
					goto STEP10 (or _StepREM if its set, i.e. from STEP00)
				
			
			*/
			

		//-----------------------------Start normal sequencer-----------------------------
			if (_Step == 0) {//STEP00 - Initialize - initialize UDP, this steps runs only once when internet is connected to begin udp
				if (_udp.begin(_localPort)) {
					_StepCMD = 10;
				}
				else {
					ErrorMessage = F("	NTP - Error - udp.begin");
					_StepREM = 0;
					_StepCMD = 98;
				}
			} else

			if (_Step == 10) {//STEP10 - Start - Does nothing
				_StepCMD = 20;
			} else

			if (_Step == 20) {//STEP20 - Send NTP Packet
				if (_SendNTPPacket()) {
					_StepCMD = 30;
					_NTPResponseTimeoutTimer = now;
				}
				else { //NTP failed
					_StepCMD = 101;
				}
			} else

			if (_Step == 30) {//STEP30 - Wait for Response
				uint32_t NTPResponse = _CheckForNTPResponse();
				if (NTPResponse > 0) {//Success
					_UnixTimeLastUpdate = NTPResponse; //Update Unix Time
					_UnixTimeLastUpdateSecond = _Seconds; //use time RIGHT NOW for most accuracy
					_SyncIntervalTimer = now; //Start next sync interval timer
					_StepCMD = 40;
					_udp.stop();//no sense in keeping this going
					ErrorMessage.clear();
				}
				if (now - _NTPResponseTimeoutTimer > 5000) {//Response Time out
					ErrorMessage = F("	NTP - Error - Server Response Timeout");
					_StepCMD = 101;
				}
			} else

			if (_Step == 40) {//STEP40 - Wait for Next Sync Interval
				if (now - _SyncIntervalTimer > SyncIntervalTime) {
					_StepCMD = 100;
				}
			} else

			if (_Step == 100) { //STEP100 - Complete - Do nothing but reset.  Used as book ends steps for completeness
				_StepCMD = 0;
			}
		//-----------------------------End normal sequencer-----------------------------



			//ERROR STEPS
			//STEP101 - STEP102 - Error step, Start a timer, Wait and retry
			if (_Step == 101) { //STEP101 - Error step, Start a timer
				_RetryTimer = now;
				_StepCMD = 102;
			} else

			if (_Step == 102 && now - _RetryTimer > 7001) { //STEP101 - Error step, Start a timer
				if (_StepREM != -1) { //If _StepREM does not equal -1 then something has set this, go back to this step;
					_StepCMD = _StepREM;
					_StepREM = -1; //Set it back to -1
				}
				else {
					_StepCMD = 10;//Go back to Start to try again
				}
			}// error steps

									
			
		}//WiFi is NOT connected or there is NO Internet
		else {
			//Stop and reset things only once, should speed things up a bit
			if (_NoWiFiONS == false) { 
				_udp.stop();
				_Step = 0;
				_StepCMD = 0;
				_StepREM = -1;
				ErrorMessage.clear();
				_NoWiFiONS = true;
				ForceTimeSyncFlag = false;
			}
		}

		//If UnixTime is valid keep it continually update
		if (_UnixTimeLastUpdate > 0) {
			UnixTime = _UnixTimeLastUpdate + (_Seconds - _UnixTimeLastUpdateSecond); //Keep Unix Time up to date
		}
		else {
			UnixTime = 0;
		}
	

	
	ForceTimeSyncFlag = false; //Reset ForceTimeSyncFlag
	_HandleTimer = now;
	} //Timer
	return (_Step != 101); //Return true (everything is good) most of the time, except for when the step number is 101
}//==================================================================================================
uint32_t NTPNonBlocking::_CheckForNTPResponse(void)
{
	int16_t size = _udp.parsePacket();
	if (size >= 48) {
		_udp.read(_packetBuffer, 48);  // read packet into the buffer
		uint32_t secsSince1900;
		uint32_t secsSince1970;
		secsSince1900 = (uint32_t)_packetBuffer[40] << 24;
		secsSince1900 |= (uint32_t)_packetBuffer[41] << 16;
		secsSince1900 |= (uint32_t)_packetBuffer[42] << 8;
		secsSince1900 |= (uint32_t)_packetBuffer[43];

		secsSince1970 = secsSince1900 - 2208988800UL; //convert UTC time to Unix time Jan 1, 1900 -> Jan 1, 1970
		//Serial.print("Unix Time: "); Serial.println(secsSince1970);
		return secsSince1970;
	}

return 0;
} //===============================================================================================
bool NTPNonBlocking::_SendNTPPacket(void) {
	//Get Unix time from NTP
	//print(F("	NTP - Sending Packet"));
	while (_udp.parsePacket() > 0); // discard any previously received packets
	IPAddress timeServerIP;
	if (WiFi.hostByName(NTPServerName.c_str(), timeServerIP) == 1) { //get IP Address from pool
		//Send NTP Packet - For more information: http://www.cisco.com/c/en/us/about/press/internet-protocol-journal/back-issues/table-contents-58/154-ntp.html
		memset(_packetBuffer, 0, 48); 	// set all bytes in the buffer to 0
		_packetBuffer[0] = 0x11100011;   // LI, Version, Mode
		_packetBuffer[1] = 0;     // Stratum, or type of clock
		_packetBuffer[2] = 6;     // Polling Interval
		_packetBuffer[3] = 0xEC;  // Peer Clock Precision
		if (_udp.beginPacket(timeServerIP, 123) == 1) {//NTP requests are to port 123
			if (_udp.write(_packetBuffer, 48) > 0) {
				if (_udp.endPacket() == 1) {
				}
				else {
					ErrorMessage = F("	NTP - ERROR - udp.endPacket Failed");
					return false;
				}
			}
			else {
				ErrorMessage = F("	NTP - ERROR - udp.write Failed");
				return false;
			}
		}
		else {
			ErrorMessage = F("	NTP - ERROR - udp.beginPacket Failed");
			return false;
		}
	}
	else {
		ErrorMessage = F("	NTP - ERROR - Failed DNS resolution of NTP Server from NTP host name");
		return false;
	}

return true;
} //===============================================================================================