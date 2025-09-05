#include "SPI.h"
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h> 
#include <BLEMidi.h>

#define TFT_DC             17
#define _sclk              5
#define _mosi              18 /* 10K pull-up */
#define _miso              19
#define TFT_CS              21 /* 10K pull-up */
#define TFT_RST            4
#define TFT_BACKLIGHT_PIN   2 /* -via transistor- */
#define TOUCH_CS_PIN       23 /* 10K pull-up */
#define TOUCH_IRQ_PIN      22

#define TFT_NORMAL          1
#define TFT_UPSIDEDOWN      3

const uint8_t TFT_ORIENTATION = TFT_NORMAL;

Adafruit_ILI9341 tft = Adafruit_ILI9341( TFT_CS, TFT_DC, TFT_RST );

XPT2046_Touchscreen touch( TOUCH_CS_PIN, TOUCH_IRQ_PIN );

void setup() {
  Serial.begin( 115200 );
  SPI.begin( _sclk, _miso, _mosi );
  SPI.setFrequency( 60000000 );

  tft.begin();

  tft.setRotation( TFT_ORIENTATION );
  tft.fillScreen( ILI9341_BLACK );
  tft.setTextColor( ILI9341_GREEN, ILI9341_BLACK );

  pinMode( TFT_BACKLIGHT_PIN, OUTPUT );
  digitalWrite( TFT_BACKLIGHT_PIN, HIGH );

  touch.begin();
  Serial.println( "Touch screen ready." );

  Serial.println("Initializing bluetooth");
  BLEMidiServer.begin("Basic MIDI device");
  Serial.println("Waiting for connections...");

  xTaskCreate(
      tftTask,       // pvTaskCode
      "TFT Task",    // pcName
      2048,         // usStackDepth (words) => ~8 KB
      nullptr,      // pvParameters
      1,            // uxPriority
      nullptr       // pxCreatedTask
  );
  xTaskCreate(
      midiTask,       // pvTaskCode
      "MIDI Task",    // pcName
      2048,         // usStackDepth (words) => ~8 KB
      nullptr,      // pvParameters
      0,            // uxPriority
      nullptr       // pxCreatedTask
  );
}


void midiTask(void * p)
{
  for (;;)
  {
    BLEMidiServer.noteOn(0, 69, 127);
    vTaskDelay(pdMS_TO_TICKS(1000));
    BLEMidiServer.noteOff(0, 69, 127);        // Then we make a delay of one second before returning to the beginning of the loop
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}



void tftTask(void * p)
{
  TS_Point rawLocation;
  for (;;)
  {
    rawLocation = touch.getPoint();
    tft.drawPixel( mapFloat(  rawLocation.x, 340, 3900, 320, 0 ),
                              mapFloat( rawLocation.y, 200, 3850, 240, 0 ),
                              ILI9341_GREEN );
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}



void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

static inline __attribute__((always_inline)) float mapFloat( float x, const float in_min, const float in_max, const float out_min, const float out_max)
{
  return ( x - in_min ) * ( out_max - out_min ) / ( in_max - in_min ) + out_min;
}