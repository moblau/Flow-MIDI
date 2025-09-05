#include "SPI.h"
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>
#include <BLEMidi.h>


#define TFT_DC             17
#define _sclk              5
#define _mosi              18
#define _miso              19
#define TFT_CS             21
#define TFT_RST            4
#define TFT_BACKLIGHT_PIN  2
#define TOUCH_CS_PIN       23
#define TOUCH_IRQ_PIN      22

#define TFT_NORMAL         1
const uint8_t TFT_ORIENTATION = TFT_NORMAL;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

// ===== Raw calibration (adjust to your panel) =====
static const int RX_MIN = 500,  RX_MAX = 3700; // raw X
static const int RY_MIN = 350,  RY_MAX = 3600; // raw Y



// MIDI config
static const uint8_t MIDI_CH = 0;  // 0 = channel 1
static const uint8_t CC_X    = 20;
static const uint8_t CC_Y    = 21;

// Inter-task data
struct TouchState { uint8_t x; uint8_t y; bool pressed; };
QueueHandle_t touchQ;

// Map and clamp helper
static inline uint8_t mapToCC(int raw, int inMin, int inMax, bool invert=false) {
  // map to 0..127
  float val = (float)(raw - inMin) * 127.0f / (float)(inMax - inMin);
  if (invert) val = 127.0f - val;
  if (val < 0) val = 0;
  if (val > 127) val = 127;
  return (uint8_t)val;
}

void tftTask(void *p) {
  // optional: draw + poll
  tft.fillScreen(ILI9341_BLACK);
  int16_t lastX = -1;
  int16_t lastY = -1;
  int8_t resetCounter = 0;
  for (;;) {
    TouchState ts;
    if (touch.touched()) {
      
      auto pt = touch.getPoint();     // raw.x, raw.y
      // NOTE: Depending on rotation/panel, you might need to swap axes.
      // Try this first; if flipped, swap raw→x/y or set invert flags.
      ts.x = mapToCC(pt.x, RX_MIN, RX_MAX, /*invert=*/true);  // left=0, right=127
      ts.y = mapToCC(pt.y, RY_MIN, RY_MAX, /*invert=*/true);  // top=0, bottom=127
      Serial.println("touched at " + String(ts.x) + " " + String(ts.y));
      ts.pressed = true;

      // (Optional) visualize touch
      int16_t px = map(pt.x, RX_MIN, RX_MAX, 320, 0);
      int16_t py = map(pt.y, RY_MIN, RY_MAX, 240, 0);

      if (lastX = -1)
      {
        lastX = px;
        lastY = py;
      }

      if ( abs(px - lastX) > 10 || abs(py - lastY) > 10){
        continue;
      }

      lastX = px;
      lastY = py;

      if (px >= 0 && px < 320 && py >= 0 && py < 240)
        tft.drawPixel(px, py, ILI9341_GREEN);
      } else {
        resetCounter++;
        if (resetCounter == 12){
          resetCounter = 0;
          ts.x = 63; ts.y = 63; ts.pressed = false;
        }
        
      }

    // Non-blocking send; if queue is full, overwrite oldest by using xQueueOverwrite on an overwrite queue
    xQueueSend(touchQ, &ts, 0);

    // ~100 Hz scan
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void midiTask(void *p) {
  TouchState ts;
  uint8_t lastX = 255, lastY = 255;  // impossible initial values
  for (;;) {
    if (xQueueReceive(touchQ, &ts, pdMS_TO_TICKS(20))) {
      // Only send if changed to reduce BLE traffic
      Serial.println("received ts");
      if (ts.x != lastX) {
        BLEMidiServer.controlChange(MIDI_CH, CC_X, ts.x);
        Serial.println("Sending X: " + String(ts.x));
        lastX = ts.x;
      }
      if (ts.y != lastY) {
        BLEMidiServer.controlChange(MIDI_CH, CC_Y, ts.y);
        Serial.println("Sending Y: " + String(ts.y));
        lastY = ts.y;
      }
    } else {
      // Timeout: yield anyway
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin(_sclk, _miso, _mosi);
  // Let each lib manage SPI transactions; no global 60 MHz needed for both TFT and touch

  tft.begin();
  tft.setRotation(TFT_ORIENTATION);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  touch.begin();
  Serial.println("Touch screen ready.");

  BLEMidiServer.begin("Flow BLE MIDI");
  Serial.println("BLE MIDI ready; waiting for connection…");

  touchQ = xQueueCreate(4, sizeof(TouchState)); // small mailbox is fine

  // Pin UI to core 1; leave core 0 freer for BLE
  xTaskCreatePinnedToCore(tftTask,  "TFT Task",  4096, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(midiTask, "MIDI Task", 4096, nullptr, 1, nullptr, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}