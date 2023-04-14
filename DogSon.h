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

byte ip0, ip1, ip2, ip3;//ip地址
ushort port;//端口
String ssid, pass;//wifi 信息

static CxgCommand command;
#if defined USEUDP
WiFiUDP udpClient;
bool udpBegin;
#else
WiFiClient client;//声明一个ESP32客户端对象，用于与服务器进行连接
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
	//-------------------存储相关------------------------
	EEPROMInit();
	ReadNodeID();
	ReadWifi();
	ReadServer();

	//-------------------串口相关------------------------
	Serial.begin(ControlBaud);
	Serial.println("----------------INFO--------------------");
	Serial.print("DODSON:");
	Serial.println(VER);
	Serial.printf("\n Group:%u NodeID:%u\n", group, nodeid);
	Serial.printf("\n WIFI: '%s'(%u), '%s'(%u)\n", ssid.c_str(), ssid.length(), pass.c_str(), pass.length());
	Serial.printf("\n IP:%u.%u.%u.%u Port:%u\n", ip0, ip1, ip2, ip3, port);


	//-------------------WIFI相关------------------------
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(true); //关闭STA模式下wifi休眠，提高响应速度

	WiFi.begin(ssid.c_str(), pass.c_str()); //连接网络

	//-------------------命令协议相关------------------------
	command.setBufferSize(20);//设置缓冲区大小, 预计指令的最大的长度
	byte start[2] = { 0xff, 0xfe };
	command.setStart(start, 2);//设置开始指令开始的匹配
	byte end[2] = { 0xfd, 0xfc };
	command.setEnd(end, 2);//设置开始指令结束的匹配

	//设置指令处理回调
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

	//设置发送数据的回调实现
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
	if (WiFi.status() == WL_CONNECTED)//连接成功的情况
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
		if (client.connect(serverIP, port)) //尝试访问目标地址
		{
			Serial.println("Connect success!");
			//client.print("Hello world!");                    //向服务器发送数据

			while (client.connected() || client.available()) //如果已连接或有收到的未读取的数据
			{
				if (client.available()) //如果有数据可读取
				{
					//接收指令, 直接跟WIFI结合
					command.addData(client.read());
				}

				//============处理检测逻辑完场后准备发送数据======================
				if (nodeLoopPtr != NULL)
				{
					nodeLoopPtr();
				}
				Setting();
			}
			Serial.println("Connect closed!");
			client.stop(); //关闭客户端
		}
		else
		{
			Serial.println("Connect Fail");
			client.stop(); //关闭客户端
			Setting();
		}
		delay(5000);
#endif
	}
	else {//wifi没有连接的情况
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
/// 设置
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
			ip0 = Serial.readStringUntil('.').toInt();        // 存储ip地址
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

