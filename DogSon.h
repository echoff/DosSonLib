// DogSon.h

#ifndef _DOGSON_h
#define _DOGSON_h

#define DOGSONVER 0x14
#define ControlBaud 256000
#define ReconnWiFiTimes 3
#define ReconnWiFiInterval 10000 //����WIFI���ʱ��

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <WiFi.h>

#if defined USEUDP
#include<WiFiUdp.h>
#define UDPPort 56050
#endif

#include <cxg_Command.h>
#include <EEPROM.h>

#if defined USEBtn
#include <Bounce2.h>
Bounce2::Button button = Bounce2::Button();
#endif

#if defined USERGBLED
	#if defined NEWBORAD
	uint8_t ledr=64, ledg=0, ledb=0;
	#else
	#include <Adafruit_NeoPixel.h>
	Adafruit_NeoPixel pixels(1, D3, NEO_GRB + NEO_KHZ800);
	uint32_t ledRedColor = pixels.Color(100, 0, 0);
	uint32_t ledGreenColor = pixels.Color(0, 100, 0);
	uint32_t ledYellowColor = pixels.Color(100, 100, 0);
	uint32_t ledColor = ledRedColor;
	#endif

bool rgbLedOn = false;
unsigned long rgbLedBlinkTimer = 0;
byte ledBlinkCount = 0;
byte ledBlinkNum = 0;
#endif

EEPROMClass  IPAndPort("eeprom0");
EEPROMClass  WIFIName("eeprom1");
EEPROMClass  WIFIPass("eeprom2");
EEPROMClass  NODEID("eeprom3");
EEPROMClass  CustomSave("eeprom4");

byte group, nodeid;

byte ip0, ip1, ip2, ip3;//ip��ַ
ushort port;//�˿�
String ssid, pass;//wifi ��Ϣ

byte reconnectWIFICount = 0;
/// <summary>
/// �������ͼ��
/// </summary>
byte heartbeatPacketInterval;
/// <summary>
/// ��������wifi�����ʱ��
/// </summary>
unsigned long reconnectWIFITimer = 0;
/// <summary>
/// ���������ͼ����ʱ��
/// </summary>
unsigned long sendHeartbeatPacketTimer = 0;

static CxgCommand command;
#if defined USEUDP
WiFiUDP udpClient;
bool udpBegin;
#else
WiFiClient client;//����һ��ESP32�ͻ��˶����������������������
#endif
bool settingOK = false;

void (*nodeLoopPtr)() = NULL;
void (*customSettingPrt)(char c) = NULL;
void (*customInfoPrt)() = NULL;
void (*onMsgPrt)(byte* data, int lenght) = NULL;

void DogSonSetup();
void DogSonUpdate();

void EEPROMInit();
void ReadNodeID();
void ReadWifi();
void ReadServer();

void SaveWifi();
void SaveNodeID();
void SaveServer();

void Setting();
void WiFiEvent(WiFiEvent_t);

#ifdef USEBatA0
float GetBat();
#endif

void DogSonSetup()
{
#ifdef USEBatA0
	pinMode(A0, INPUT);
#endif

#if defined USERGBLED
	#if defined NEWBORAD
		digitalWrite(RGB_BUILTIN, HIGH);
	#else
		pixels.begin();
		pixels.setPixelColor(0, pixels.Color(10,10,10));
		pixels.show();
	#endif
#endif

#if defined USEBtn
	//pinMode(D1, INPUT_PULLUP);
	button.attach(9, INPUT_PULLUP);
	button.interval(5);
	button.setPressedState(LOW);
#endif
	//-------------------�洢���------------------------
	EEPROMInit();
	ReadNodeID();
	ReadWifi();
	ReadServer();

	//-------------------�������------------------------
	Serial.begin(ControlBaud);
	Serial.printf("DODSON:%X,NODE:%X\n", DOGSONVER, VER);
	Serial.printf("HW:%X\n", HARDWARETYPE);
	Serial.printf("Group:%u NodeID:%u\n", group, nodeid);
	Serial.printf("WIFI: '%s'(%u), '%s'(%u)\n", ssid.c_str(), ssid.length(), pass.c_str(), pass.length());
	Serial.printf("ServerIP:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);


	//-------------------WIFI���------------------------
	WiFi.mode(WIFI_STA);
#ifdef SAVE_POWER
	WiFi.setSleep(true);
#else
	WiFi.setSleep(false); //�ر�STAģʽ��wifi���ߣ������Ӧ�ٶ�
#endif // SAVE_POWER

	
	WiFi.onEvent(WiFiEvent);


	WiFi.begin(ssid.c_str(), pass.c_str()); //��������

	//-------------------����Э�����------------------------
	command.setBufferSize(512);//���û�������С, Ԥ��ָ������ĳ���
	byte start[2] = { 0xff, 0xfe };
	command.setStart(start, 2);//���ÿ�ʼָ�ʼ��ƥ��
	byte end[2] = { 0xfd, 0xfc };
	command.setEnd(end, 2);//���ÿ�ʼָ�������ƥ��

	//����ָ���ص�
	command.setResolveCommandCallback([](byte* buff, int startIndex, int length) {
		int endIndex = startIndex + length;
		/*Serial.print("receive Command: ");
		for (int i = startIndex; i < endIndex; i++) {
			Serial.print(*(buff + i), HEX);
			Serial.print(" ");
		}
		Serial.println("\n");*/

		byte cmdData[length];
		memcpy(cmdData, buff + startIndex, length);

		if (onMsgPrt != NULL)
		{
			onMsgPrt(cmdData, length);
		}

		byte cmdType = cmdData[0];

		byte selfData[10];
		switch (cmdType)
		{
		case 0x1:
			selfData[0] = HARDWARETYPE;
			selfData[1] = group;
			selfData[2] = nodeid;
			selfData[3] = 0x01;
			selfData[4] = WiFi.RSSI();
			command.sendCommand(selfData, 5);
			break;
		case 0x3:
			selfData[0] = HARDWARETYPE;
			selfData[1] = group;
			selfData[2] = nodeid;
			selfData[3] = 0x03;
			selfData[4] = VER;
			command.sendCommand(selfData, 5);
			break;
		case 0x4:
			selfData[0] = HARDWARETYPE;
			selfData[1] = group;
			selfData[2] = nodeid;
			selfData[3] = 0x04;
			selfData[4] = DOGSONVER;
			command.sendCommand(selfData, 5);
			break;
#ifdef USEBatA0
		case 0x5:
			float Vbattf = GetBat();
			selfData[0] = HARDWARETYPE;
			selfData[1] = group;
			selfData[2] = nodeid;
			selfData[3] = 0x05;
			selfData[4] = *((byte*)(&Vbattf) + 0);
			selfData[5] = *((byte*)(&Vbattf) + 1);
			selfData[6] = *((byte*)(&Vbattf) + 2);
			selfData[7] = *((byte*)(&Vbattf) + 3);
			command.sendCommand(selfData, 8);
			break;
#endif
		case 0x6:
			ip0 = cmdData[1];
			ip1 = cmdData[2];
			ip2 = cmdData[3];
			ip3 = cmdData[4];
			port = (cmdData[5] << 8) | cmdData[6];
			Serial.printf("\n IP:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);
			SaveServer();
			break;
		}
	});

	//���÷������ݵĻص�ʵ��
	command.setSendCommandCallback([](byte* buff, int length) {
#if defined USEUDP
		IPAddress serverIP(ip0, ip1, ip2, ip3);
		udpClient.beginPacket(serverIP, port);
		udpClient.write(buff, length);
		udpClient.endPacket();
#else
		client.write(buff, length);
#endif
		});
}

void DogSonUpdate()
{
#if defined USEBtn
	button.update();
	if (button.pressed()) {
		Serial.println("Btn Trigger");
		ledBlinkNum = 3;
		rgbLedBlinkTimer = 0;
		ledBlinkCount = 0;
	}
#endif

	if (WiFi.status() == WL_CONNECTED)//���ӳɹ������
	{
#if defined USEUDP
		if (!udpBegin)
		{
			udpClient.begin(WiFi.localIP(), UDPPort);
			udpBegin = true;
		}
		else {
			//��ȡUDP����
			int packetSize = udpClient.parsePacket();
			if (packetSize)
			{
				//Serial.print("Receive:");
				uint8_t buf[packetSize];
				udpClient.read(buf, packetSize);
				//udpClient.readBytes(buf, packetSize);
				for (int i = 0; i < packetSize; i++)
				{
					//Serial.print(buf[i], HEX);
					//Serial.print('.');
					command.addData(buf[i]);
				}
				//Serial.println();
			}
			//����������
			if (heartbeatPacketInterval > 0)
			{
				unsigned long currentMillis = millis();
				if (currentMillis - sendHeartbeatPacketTimer >= heartbeatPacketInterval * 1000)
				{
#ifdef  USEBatA0 
#ifdef REPORTBat

					float Vbattf = GetBat();
					byte selfData[9];
					selfData[0] = HARDWARETYPE;
					selfData[1] = group;
					selfData[2] = nodeid;
					selfData[3] = 0xAA;
					selfData[4] = WiFi.RSSI();
					selfData[5] = *((byte*)(&Vbattf) + 0);
					selfData[6] = *((byte*)(&Vbattf) + 1);
					selfData[7] = *((byte*)(&Vbattf) + 2);
					selfData[8] = *((byte*)(&Vbattf) + 3);
					command.sendCommand(selfData, 9);
#endif
#else
					byte selfData[5] = { HARDWARETYPE,group,nodeid,0xAA,WiFi.RSSI() };
					command.sendCommand(selfData, 5);
#endif 
					sendHeartbeatPacketTimer = currentMillis;
				}
			}

			//���ÿͻ�ѭ��
			if (nodeLoopPtr != NULL)
			{
				nodeLoopPtr();
			}
			//�������÷���
			Setting();
		}

#else
		
		IPAddress serverIP(ip0, ip1, ip2, ip3);
		if (client.connect(serverIP, port)) //���Է���Ŀ���ַ
		{
			Serial.println("Connect success!");
			//client.print("Hello world!");    
			//���������������
			while (client.connected() || client.available()) //��������ӻ����յ���δ��ȡ������
			{
				if (client.available()) //��������ݿɶ�ȡ
				{
					//����ָ��, ֱ�Ӹ�WIFI���
					command.addData(client.read());
				}

				//============�������߼��곡��׼����������======================
				if (nodeLoopPtr != NULL)
				{
					nodeLoopPtr();
				}
				//����������
				if (heartbeatPacketInterval > 0)
				{
					unsigned long currentMillis = millis();
					if (currentMillis - sendHeartbeatPacketTimer >= heartbeatPacketInterval * 1000)
					{
						byte selfData[5] = { HARDWARETYPE,group,nodeid,0xAA,WiFi.RSSI() };
						command.sendCommand(selfData, 5);
						sendHeartbeatPacketTimer = currentMillis;
					}
				}
				Setting();
			}
			Serial.println("Connect closed!");
			client.stop(); //�رտͻ���
		}
		else
		{
			Serial.println("Connect Fail");
			client.stop(); //�رտͻ���
			Setting();
		}
		delay(5000);
#endif
	}
	else {//wifiû�����ӵ����
		Setting();
		unsigned long currentMillis = millis();
		// ��ʱ����
		if (currentMillis - reconnectWIFITimer >= ReconnWiFiInterval) {
			if (reconnectWIFICount >= ReconnWiFiTimes)
			{
				reconnectWIFICount = 0;
				Serial.println("Restart...");
				ESP.restart();
			}
			else {
				reconnectWIFICount++;
				Serial.println("Reconnecting to WiFi...");
				WiFi.begin(ssid.c_str(), pass.c_str());
				reconnectWIFITimer = currentMillis;
			}
		}
	}
#if defined USERGBLED
	if (ledBlinkCount < ledBlinkNum)
	{
		if (millis() - rgbLedBlinkTimer > 300)
		{
			if (rgbLedOn)
			{
				#if defined NEWBORAD
				neopixelWrite(RGB_BUILTIN, 0, 0, 0);
				#else
				pixels.setPixelColor(0, 0);
				pixels.show();
				#endif
				rgbLedOn = false;
				ledBlinkCount++;
			}
			else {
				#if defined NEWBORAD
				neopixelWrite(RGB_BUILTIN,ledr,ledg,ledb);
				#else
				pixels.setPixelColor(0, ledColor);
				pixels.show();
				#endif
				rgbLedOn = true;
			}
			rgbLedBlinkTimer = millis();
		}		
	}
#endif
}

void EEPROMInit()
{
	if (!NODEID.begin(50)) {
		ESP.restart();
	}
	if (!IPAndPort.begin(50)) {
		ESP.restart();
	}
	if (!WIFIName.begin(50)) {
		ESP.restart();
	}
	if (!WIFIPass.begin(50)) {
		ESP.restart();
	}
	if (!CustomSave.begin(50)) {
		ESP.restart();
	}
}

void ReadWifi()
{
	WIFIName.get(0, ssid);
	WIFIPass.get(0, pass);
}

void ReadServer()
{
	IPAndPort.get(0, ip0);
	IPAndPort.get(1, ip1);
	IPAndPort.get(2, ip2);
	IPAndPort.get(3, ip3);
	byte gPortH = 0;
	byte gPortL = 0;
	IPAndPort.get(4, gPortH);
	IPAndPort.get(5, gPortL);
	port = (gPortH << 8) | gPortL;
}

void ReadNodeID()
{
	NODEID.get(0, group);
	NODEID.get(1, nodeid);
	NODEID.get(2, heartbeatPacketInterval);
}

void SaveWifi()
{
	WIFIName.writeString(0, ssid);
	WIFIPass.writeString(0, pass);
	WIFIName.commit();
	WIFIPass.commit();
}

void SaveServer()
{
	IPAndPort.put(0, ip0);
	IPAndPort.put(1, ip1);
	IPAndPort.put(2, ip2);
	IPAndPort.put(3, ip3);
	byte portH = (port >> 8) & 0xff;
	byte portL = port & 0xff;
	IPAndPort.put(4, portH);
	IPAndPort.put(5, portL);
	IPAndPort.commit();

	ESP.restart();
}

void SaveNodeID()
{
	NODEID.put(0, group);
	NODEID.put(1, nodeid);
	NODEID.put(2, heartbeatPacketInterval);
	NODEID.commit();
}

/// <summary>
/// ����
/// </summary>
void Setting()
{
	while (Serial.available() > 0)
	{
		char c = Serial.read();
		switch (c)
		{
		case 'W'://set wifi
			Serial.printf("\n W? Change Wifi SSID Password!\n");
			while (Serial.read() != -1); // discard all other received characters
			Serial.printf("\nEnter <SSID Password> (with the <>!)\n");
			while (Serial.available()==0);
			// read in the user input
			Serial.readStringUntil('<'); // ignore everything up to <  (SSID will be overwritten next)
			ssid = Serial.readStringUntil(' ');        // store SSID
			pass = Serial.readStringUntil('>');        // store password
			while (Serial.read() != -1); // discard the rest of the input
			Serial.printf("\n Going to '%s'(%u), '%s'(%u)\n", ssid.c_str(), ssid.length(), pass.c_str(), pass.length());
			SaveWifi();
			break;
		case 'S'://set server
			Serial.printf("\n W? Change ServerIP and Port!\n");
			while (Serial.read() != -1); // discard all other received characters
			Serial.printf("\nEnter <xxx.xxx.xxx.xxx:Port> (with the <>!)\n");
			while (Serial.available()==0);
			// read in the user input
			Serial.readStringUntil('<');
			ip0 = Serial.readStringUntil('.').toInt();        // �洢ip��ַ
			ip1 = Serial.readStringUntil('.').toInt();
			ip2 = Serial.readStringUntil('.').toInt();
			ip3 = Serial.readStringUntil(':').toInt();
			port = Serial.readStringUntil('>').toInt();
			while (Serial.read() != -1); // discard the rest of the input
			Serial.printf("\n IP:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);
			SaveServer();
			break;
		case 'G':
			Serial.printf("\n G? Set Group And NodeID\n");
			while (Serial.read() != -1); // discard all other received characters
			Serial.printf("\nEnter <group.nodeid.heartbeatPacketInterval> (with the <>!)\n");
			while (Serial.available()==0);
			// read in the user input
			Serial.readStringUntil('<');
			group = Serial.readStringUntil('.').toInt();
			nodeid = Serial.readStringUntil('.').toInt();
			heartbeatPacketInterval = Serial.readStringUntil('>').toInt();
			while (Serial.read() != -1); // discard the rest of the input
			Serial.printf("\n Group:%u NodeID:%u HeartbeatPacketInterval:%u\n", group, nodeid, heartbeatPacketInterval);
			SaveNodeID();
			break;
		case 'I'://get info
			Serial.println();
			Serial.printf("====Info====\n");
			while (Serial.read() != -1); // discard all other received characters
			Serial.printf("HW:%X\n", HARDWARETYPE);
			uint64_t chipid;
			chipid = ESP.getEfuseMac();
			Serial.printf("ESP32 Chip ID = %04X", (ushort)(chipid >> 32)); //print High 2 bytes
			Serial.printf("%08X\n", (uint32_t)chipid); //print Low 4bytes.
			Serial.printf("DODSON:%X,NODE:%X\n", DOGSONVER, VER);
			Serial.printf("Group:%u NodeID:%u HPI:%u\n", group, nodeid, heartbeatPacketInterval);
			Serial.println("====WIFI====");
			Serial.printf("WIFI: '%s'(%u), '%s'(%u)\n", ssid.c_str(), ssid.length(), pass.c_str(), pass.length());
			Serial.printf("WIFI Status:%u LocalIP:%s RSSI:%d\n", WiFi.status(), WiFi.localIP().toString(), WiFi.RSSI());
			Serial.println("====Server====");
			Serial.printf("Server:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);

			if (customInfoPrt != NULL)
			{
				customInfoPrt();
			}
			break;
			//#if defined CUSTOM_SETTING
			//case CUSTOM_SETTING:
			//	while (Serial.read() != -1); // discard all other received characters
			//	Serial.println(CUSTOM_SETTING_PRINT);
			//	while (not Serial.available());
			//	break;
			//#endif
		default:
			if (customSettingPrt != NULL)
			{
				customSettingPrt(c);
			}
			break;
		}
	}
}

void WiFiEvent(WiFiEvent_t event)
{
	Serial.printf("[WiFi-event] event: %d\n", event);

	switch (event) {
	case SYSTEM_EVENT_WIFI_READY:
		Serial.println("WiFi interface ready");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		Serial.println("Completed scan for access points");
		break;
	case SYSTEM_EVENT_STA_START:
		Serial.println("WiFi client started");
		break;
	case SYSTEM_EVENT_STA_STOP:
		Serial.println("WiFi clients stopped");
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		Serial.println("Connected to access point");
#if defined USERGBLED
		ledBlinkNum = 3;
	#if defined NEWBORAD
		ledr = 0;
		ledg = 64;
		ledb = 0;
	#else
		ledColor = ledGreenColor;
	#endif
#endif
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		Serial.println("Disconnected from WiFi access point");
		break;
	case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
		Serial.println("Authentication mode of access point has changed");
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		Serial.print("Obtained IP address: ");
		Serial.println(WiFi.localIP());
		break;
	case SYSTEM_EVENT_STA_LOST_IP:
		Serial.println("Lost IP address and IP address is reset to 0");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
		Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_FAILED:
		Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
		Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_PIN:
		Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
		break;
	case SYSTEM_EVENT_AP_START:
		Serial.println("WiFi access point started");
		break;
	case SYSTEM_EVENT_AP_STOP:
		Serial.println("WiFi access point  stopped");
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		Serial.println("Client connected");
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		Serial.println("Client disconnected");
		break;
	case SYSTEM_EVENT_AP_STAIPASSIGNED:
		Serial.println("Assigned IP address to client");
		break;
	case SYSTEM_EVENT_AP_PROBEREQRECVED:
		Serial.println("Received probe request");
		break;
	case SYSTEM_EVENT_GOT_IP6:
		Serial.println("IPv6 is preferred");
		break;
	case SYSTEM_EVENT_ETH_START:
		Serial.println("Ethernet started");
		break;
	case SYSTEM_EVENT_ETH_STOP:
		Serial.println("Ethernet stopped");
		break;
	case SYSTEM_EVENT_ETH_CONNECTED:
		Serial.println("Ethernet connected");
		break;
	case SYSTEM_EVENT_ETH_DISCONNECTED:
		Serial.println("Ethernet disconnected");
		break;
	case SYSTEM_EVENT_ETH_GOT_IP:
		Serial.println("Obtained IP address");
		break;
	default: break;
	}}

#ifdef USEBatA0
float GetBat()
{
	uint32_t Vbatt = 0;
	for (int i = 0; i < 16; i++) {
		Vbatt = Vbatt + analogReadMilliVolts(A0); // ADC with correction   
	}
	float Vbattf = 2 * Vbatt / 16 / 1000.0;     // attenuation ratio 1/2, mV --> V
	return Vbattf;
}
#endif

#endif

