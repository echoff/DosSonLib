// DogSon.h

#ifndef _DOGSON_h
#define _DOGSON_h


#define ControlBaud 256000

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


EEPROMClass  IPAndPort("eeprom0");
EEPROMClass  WIFIName("eeprom1");
EEPROMClass  WIFIPass("eeprom2");
EEPROMClass  NODEID("eeprom3");
EEPROMClass  CustomSave("eeprom4");

byte group, nodeid;

byte ip0, ip1, ip2, ip3;//ip��ַ
ushort port;//�˿�
String ssid, pass;//wifi ��Ϣ

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

void DogSonSetup()
{
	//-------------------�洢���------------------------
	EEPROMInit();
	ReadNodeID();
	ReadWifi();
	ReadServer();

	//-------------------�������------------------------
	Serial.begin(ControlBaud);
	Serial.println("----------------INFO--------------------");
	Serial.print("DODSON:");
	Serial.println(VER);
	Serial.printf("\n Group:%u NodeID:%u\n", group, nodeid);
	Serial.printf("\n WIFI: '%s'(%u), '%s'(%u)\n", ssid.c_str(), ssid.length(), pass.c_str(), pass.length());
	Serial.printf("\n IP:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);


	//-------------------WIFI���------------------------
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(true); //�ر�STAģʽ��wifi���ߣ������Ӧ�ٶ�

	WiFi.begin(ssid.c_str(), pass.c_str()); //��������

	//-------------------����Э�����------------------------
	command.setBufferSize(20);//���û�������С, Ԥ��ָ������ĳ���
	byte start[2] = { 0xff, 0xfe };
	command.setStart(start, 2);//���ÿ�ʼָ�ʼ��ƥ��
	byte end[2] = { 0xfd, 0xfc };
	command.setEnd(end, 2);//���ÿ�ʼָ�������ƥ��

	//����ָ���ص�
	command.setResolveCommandCallback([](byte* buff, int startIndex, int length) {
		int endIndex = startIndex + length;
		Serial.println("receive Command: ");

		for (int i = startIndex; i < endIndex; i++) {
			Serial.print(*(buff + i), HEX);
			Serial.print(" ");
		}
		Serial.println("\n");
		byte data = *(buff + startIndex);
		if (data == 1)
		{
			byte selfData[1] = { WiFi.RSSI() };
			command.sendCommand(selfData, 1);
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
	if (WiFi.status() == WL_CONNECTED)//���ӳɹ������
	{
#if defined USEUDP
		if (!udpBegin)
		{
			udpClient.begin(WiFi.localIP(), UDPPort);
			udpBegin = true;
		}
		else {
			if (nodeLoopPtr != NULL)
			{
				nodeLoopPtr();
			}
			Setting();
		}

#else
		IPAddress serverIP(ip0, ip1, ip2, ip3);
		if (client.connect(serverIP, port)) //���Է���Ŀ���ַ
		{
			Serial.println("Connect success!");
			//client.print("Hello world!");                    //���������������

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
	}
}

void EEPROMInit()
{
	if (!NODEID.begin(50)) {
		Serial.println("Failed to initialise WIFIPass");
		Serial.println("Restarting...");
		delay(1000);
		ESP.restart();
	}
	if (!IPAndPort.begin(50)) {
		Serial.println("Failed to initialise IPAndPort");
		Serial.println("Restarting...");
		delay(1000);
		ESP.restart();
	}
	if (!WIFIName.begin(50)) {
		Serial.println("Failed to initialise WIFIName");
		Serial.println("Restarting...");
		delay(1000);
		ESP.restart();
	}
	if (!WIFIPass.begin(50)) {
		Serial.println("Failed to initialise WIFIPass");
		Serial.println("Restarting...");
		delay(1000);
		ESP.restart();
	}
	if (!CustomSave.begin(50)) {
		Serial.println("Failed to initialise WIFIPass");
		Serial.println("Restarting...");
		delay(1000);
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
}

void SaveNodeID()
{
	NODEID.put(0, group);
	NODEID.put(1, nodeid);
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
			while (not Serial.available());
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
			while (not Serial.available());
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
			Serial.printf("\nEnter <group.nodeid> (with the <>!)\n");
			while (not Serial.available());
			// read in the user input
			Serial.readStringUntil('<');
			group = Serial.readStringUntil('.').toInt();
			nodeid = Serial.readStringUntil('>').toInt();
			while (Serial.read() != -1); // discard the rest of the input
			Serial.printf("\n Group:%u NodeID:%u\n", group, nodeid);
			SaveNodeID();
			break;
		case 'I'://get info
			Serial.printf("\n I? Info\n");
			while (Serial.read() != -1); // discard all other received characters
			Serial.printf("\n Group:%u NodeID:%u\n", group, nodeid);
			Serial.printf("\n WIFI: '%s'(%u), '%s'(%u)\n", ssid.c_str(), ssid.length(), pass.c_str(), pass.length());
			Serial.printf("\n WIFI Status:%u LocalIP:%s\n", WiFi.status(), WiFi.localIP().toString());
			Serial.printf("\n RSSI:%d\n", WiFi.RSSI());
			Serial.printf("\n IP:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);
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

#endif

