#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SSD1306Wire.h>
#include <OLEDDisplayUi.h>
#include "images.h"

/* Definitions */
#define WIFI_SSID ""
#define WIFI_PWD ""
#define REGION "tenan"
#define COUNTRY "kr"
#define APPID ""

#define TIME_ZONE 9
#define DAYTIME_SAVING 0
#define REGION_NAME_LEN 30

/* Weather information structure */
typedef struct
{
	char name[REGION_NAME_LEN];
	double temp;
	int humidity;
	int temp_min;
	int temp_max;
	int speed;
	int direction;
	int conditionId;
} _weatherinfo;

/* Prototypes */
bool initWiFi(void);
void initTime(void);
void requestWeatherInfo(void);
void parseWeatherJson(String);
const uint8_t* getWeatherIcon(int conditionId);
void regionOverlay(OLEDDisplay *, OLEDDisplayUiState *);
void nameOverlay(OLEDDisplay *, OLEDDisplayUiState *);
void drawFrame1(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);
void drawFrame2(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);
void drawFrame3(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);
void drawFrame4(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);
void drawFrame5(OLEDDisplay *, OLEDDisplayUiState *, int16_t, int16_t);

/* Variables */
_weatherinfo weatherinfo;
SSD1306Wire display(0x3c, D3, D5);
OLEDDisplayUi ui(&display);

int frameCount = 5;
int overlaysCount = 2;
FrameCallback frames[] = {drawFrame1, drawFrame2, drawFrame3, drawFrame4, drawFrame5};
OverlayCallback overlays[] = {nameOverlay, regionOverlay};
char overlayName[30];

char strTime[6]; /* e.g. 12:12\0 */
bool tickerFlag = false;
Ticker ticker;

/* Overlay for region info. */
void regionOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
	display->setTextAlignment(TEXT_ALIGN_RIGHT);
	display->setFont(ArialMT_Plain_10);
	display->drawString(128, 0, weatherinfo.name);
}

/* Overlay for frame names */
void nameOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	display->setFont(ArialMT_Plain_10);
	display->drawString(0, 0, overlayName);
}

/* Current Time */
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
	memset(overlayName, 0, sizeof(overlayName));
	strncpy(overlayName, "Time", strlen("Time"));

	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_24);
	display->drawString(64 + x, 20 + y, strTime);
}

/* Weater Icon */
void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{	
	memset(overlayName, 0, sizeof(overlayName));
	strncpy(overlayName, "Weather", strlen("Weather"));
	
	display->drawXbm(x + 46, y + 14, 36, 36, getWeatherIcon(weatherinfo.conditionId));
}

/* Current Temperature */
void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
	memset(overlayName, 0, sizeof(overlayName));
	strncpy(overlayName, "Temperature", strlen("Temperature"));
	
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_24);
	display->drawString(64 + x, 20 + y, String(weatherinfo.temp) + " C");
}

/* Current Humidity */
void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
	memset(overlayName, 0, sizeof(overlayName));
	strncpy(overlayName, "Humidity", strlen("Humidity"));
	
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_24);
	display->drawString(64 + x, 20 + y, String(weatherinfo.humidity) + " %");
}

/* Wind Speed & Direction */
void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
	memset(overlayName, 0, sizeof(overlayName));
	strncpy(overlayName, "Wind Speed/Dir.", strlen("Wind Speed/Dir."));
	
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_24);
	display->drawString(64 + x, 20 + y, String(weatherinfo.speed) + " / " + String(weatherinfo.direction));
}

/* Set flag for weather information request */
void tickerCallback()
{
	tickerFlag = true;
}

void setup()
{
	Serial.begin(115200);
	//Serial.setDebugOutput(true);

	/* Initialize WiFi */
	if (!initWiFi())
		return;

	/* Initialize Time */
	initTime();

	/* Reuqest info */
	requestWeatherInfo();

	/* tickerCallback is called every minute */
	ticker.attach(60, tickerCallback);

	/* Set frame per second, max value is 60 */
	ui.setTargetFPS(30);

	/* Set symbol imamge */
	ui.setActiveSymbol(activeSymbol);
	ui.setInactiveSymbol(inactiveSymbol);

	/* Set indicator postion (TOP, LEFT, BOTTOM, RIGHT) */
	ui.setIndicatorPosition(BOTTOM);

	/* Set indicator moving direction ( LEFT_RIGHT, RIGHT_LEFT) */
	ui.setIndicatorDirection(LEFT_RIGHT);

	/* Set slide direction (SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN) */
	ui.setFrameAnimation(SLIDE_LEFT);

	/* Add frames */
	ui.setFrames(frames, frameCount);

	/* Add overlays */
	ui.setOverlays(overlays, overlaysCount);

	/* Init UI object */
	ui.init();

	/* flip display */
	display.flipScreenVertically();
}

void loop()
{
	int remainingTimeBudget = ui.update();

	if (remainingTimeBudget > 0)
	{
		if (tickerFlag)
		{
			/* Get current time */
			time_t now = time(nullptr);
			tm *local = localtime(&now);
			sprintf(strTime, "%02d:%02d", local->tm_hour, local->tm_min);

			/* request weather info */
			requestWeatherInfo();

			/* Check RAM */
			Serial.print(F("Free Heap: "));
			Serial.println(ESP.getFreeHeap());

			tickerFlag = false;
		}

		delay(remainingTimeBudget);
	}
}

bool initWiFi()
{
	Serial.println();

	if (!WiFi.begin(WIFI_SSID, WIFI_PWD))
	{
		Serial.println("ERROR: WiFi.begin");
		return false;
	}

	Serial.println("OK: WiFi.begin");

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(100);
		Serial.print(".");
	}

	Serial.println();
	Serial.println("OK: WiFi connected");

	delay(1000);

	return true;
}

void initTime()
{
	/* Congiure Time */
	Serial.println("Initialize Time...");

	configTime(TIME_ZONE * 3600, DAYTIME_SAVING, "pool.ntp.org", "time.nist.gov");

	while (!time(nullptr))
	{
		Serial.print(".");
		delay(1000);
	}

	time_t now = time(nullptr);
	tm *local = localtime(&now);
	sprintf(strTime, "%02d:%02d", local->tm_hour, local->tm_min);

	Serial.println("OK: initTime");
}

void requestWeatherInfo()
{
	HTTPClient httpClient;
	httpClient.setTimeout(2000);

	/* Connect & Request */
	String url = String("/data/2.5/weather?q=") + String(REGION) + String(",") + String(COUNTRY) + String("&units=metric&appid=") + String(APPID);
	if (!httpClient.begin("api.openweathermap.org", 80, url.c_str()))
	{
		Serial.println("ERROR: HTTPClient.begin");
		return;
	}

	Serial.println("OK: HTTPClient.begin");

	int httpCode = httpClient.GET();

	/* Check response */
	if (httpCode > 0)
	{
		Serial.printf("[HTTP] request from the client was handled: %d\n", httpCode);
		String payload = httpClient.getString();
		parseWeatherJson(payload);
	}
	else
	{
		Serial.printf("[HTTP] connection failed: %s\n", httpClient.errorToString(httpCode).c_str());
	}

	httpClient.end();
}

void parseWeatherJson(String buffer)
{
	int JsonStartIndex = buffer.indexOf('{');
	int JsonLastIndex = buffer.lastIndexOf('}');

	/* Substring JSON string */
	String JsonStr = buffer.substring(JsonStartIndex, JsonLastIndex + 1);
	Serial.println("PARSE JSON WEATHER INFORMATION: " + JsonStr);

	/* Parse JSON string */
	DynamicJsonBuffer jsonBuffer;
	JsonObject &root = jsonBuffer.parseObject(JsonStr);

	if (root.success())
	{
		/* Get information */
		weatherinfo.temp = root["main"]["temp"];
		weatherinfo.humidity = root["main"]["humidity"];
		weatherinfo.temp_min = root["main"]["temp_min"];
		weatherinfo.temp_max = root["main"]["temp_max"];
		weatherinfo.speed = root["wind"]["speed"];
		weatherinfo.direction = root["wind"]["direction"];
		weatherinfo.conditionId = root["weather"][0]["id"];
		const char *name = root["name"];
		int namelen = strlen(name);
		strncpy(weatherinfo.name, root["name"], namelen > REGION_NAME_LEN ? REGION_NAME_LEN : namelen);

		/* Serial Output */
		Serial.printf("Name: %s\r\n", weatherinfo.name);
		Serial.printf("Temp: %3.1f\r\n", weatherinfo.temp);
		Serial.printf("Humidity: %d\r\n", weatherinfo.humidity);
		Serial.printf("Min. Temp: %d\r\n", weatherinfo.temp_min);
		Serial.printf("Max. Temp: %d\r\n", weatherinfo.temp_max);
		Serial.printf("Wind Speed: %d\r\n", weatherinfo.speed);
		Serial.printf("Wind Direction: %d\r\n", weatherinfo.direction);
		Serial.printf("ConditionId: %d\r\n", weatherinfo.conditionId);
	}
	else
	{
		Serial.println("jsonBuffer.parseObject failed");
	}
}


const uint8_t* getWeatherIcon(int conditionId)
{
	/* Return string for conditionId */
	if(conditionId >= 200 && conditionId < 300) return STORM;
	else if(conditionId >= 300 && conditionId < 400) return RAIN;
	else if(conditionId >= 500 && conditionId < 600) return RAIN;
	else if(conditionId >= 600 && conditionId < 700) return SNOW;
	else if(conditionId >= 700 && conditionId < 800) return FOG;
	else if(conditionId == 800) return SUNNY;
	else if(conditionId > 800 && conditionId < 900) return CLOUD;
	else return SUNNY;
}
