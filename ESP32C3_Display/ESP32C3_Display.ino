#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

//=========================
// PIN DEFINITIONS
//=========================
#define TOUCH_CS  2
#define TOUCH_IRQ 9
#define LED_PIN   8  // PWM capable pin

//=========================
// DISPLAY OBJECTS
//=========================
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Preferences prefs;

//=========================
// DISPLAY DIMENSIONS
//=========================
#define WIDTH  320
#define HEIGHT 240

//=========================
// COLOR SCHEME - Modern Dark Theme
//=========================
#define C_BG          0x0000      // Pure black
#define C_BG_LIGHT    0x18C3      // Dark gray
#define C_BG_CARD     0x0841      // Card background
#define C_ACCENT      0x07FF      // Cyan
#define C_ACCENT2     0xF800      // Red for alerts
#define C_ACCENT3     0xFD20      // Orange
#define C_TEXT        0xFFFF      // White
#define C_TEXT_DIM    0x8C71      // Gray text
#define C_GRID        0x2104      // Subtle grid
#define C_GRID_MAJOR  0x3186      // Major grid
#define C_CH1         0x07FF      // Channel 1 - Cyan
#define C_CH2         0xF800      // Channel 2 - Red
#define C_BTN         0x2965      // Button color
#define C_BTN_ACTIVE  0x07FF      // Active button
#define C_SLIDER_BG   0x2104      // Slider background
#define C_SLIDER_FILL 0x07FF      // Slider fill

//=========================
// UI LAYOUT
//=========================
#define HEADER_H      50
#define FOOTER_H      40
#define GRAPH_X       5
#define GRAPH_Y       (HEADER_H + 5)
#define GRAPH_W       (WIDTH - 10)
#define GRAPH_H       (HEIGHT - HEADER_H - FOOTER_H - 15)
#define GRAPH_BOTTOM  (GRAPH_Y + GRAPH_H)

#define SLIDER_X      40
#define SLIDER_Y      170
#define SLIDER_W      240
#define SLIDER_H      25

//=========================
// ANALOG CHANNEL CONTROL
//=========================
const uint8_t analogPins[] = {0, 1};
const char* pinNames[] = {"CH-1", "CH-2"};
const uint16_t channelColors[] = {C_CH1, C_CH2};

volatile uint8_t currentPin = 0;

//=========================
// GRAPH VARIABLES
//=========================
uint8_t brightness = 180;
uint16_t graphBuffer[GRAPH_W];  // Store previous values
int graphIndex = 0;
bool graphInitialized = false;

//=========================
// SCREEN STATES
//=========================
enum ScreenState {
  SCREEN_OSCILLOSCOPE,
  SCREEN_BRIGHTNESS
};

ScreenState currentScreen = SCREEN_OSCILLOSCOPE;

//=========================
// FORWARD DECLARATIONS
//=========================
void drawOscilloscopeUI();
void drawBrightnessUI();
void updateBrightnessSlider();
void clearGraph();
void drawGraph();
void drawHeader();
void drawFooter();
void drawChannelSelector();
void drawReading(float voltage);
void drawGrid();

//=========================
// SETUP
//=========================
void setup() {
  // Apply brightness to LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  // Load saved brightness
  prefs.begin("nvstore", false);
  brightness = prefs.getUChar("bright", 180);
  prefs.end();

  // Initialize SPI for touch
  SPI.begin(4, 5, 6, TOUCH_CS);
  SPI.setFrequency(60000000);

  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  tft.setTextFont(1);

  // Initialize touch
  ts.begin();
  ts.setRotation(1);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  analogReadResolution(12);
  
  // Initialize graph buffer
  for(int i = 0; i < GRAPH_W; i++) {
    graphBuffer[i] = GRAPH_H / 2;
  }

  drawOscilloscopeUI();
  
  delay(50);
  analogWrite(LED_PIN, brightness);
}

//=========================
// MAIN LOOP
//=========================
void loop() {
  // Touch handling
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Map touch coordinates to screen (adjust these values for your display)
    int16_t tx = map(p.x, 186, 3646, 0, WIDTH-1);
    int16_t ty = map(p.y, 256, 3751, 0, HEIGHT-1);
    tx = constrain(tx, 0, WIDTH-1);
    ty = constrain(ty, 0, HEIGHT-1);

    if (currentScreen == SCREEN_OSCILLOSCOPE) {
      // Settings button (top right)
      if (tx >= WIDTH - 50 && tx <= WIDTH - 10 && ty >= 10 && ty <= HEADER_H - 10) {
        currentScreen = SCREEN_BRIGHTNESS;
        drawBrightnessUI();
      }
      // Channel selector area
      else if (tx >= 20 && tx <= 120 && ty >= 10 && ty <= HEADER_H - 10) {
        currentPin = (currentPin + 1) % 2;
        drawChannelSelector();
        clearGraph();
      }
    }
    else if (currentScreen == SCREEN_BRIGHTNESS) {
      // Back button
      if (tx >= 10 && tx <= 90 && ty >= 10 && ty <= 40) {
        currentScreen = SCREEN_OSCILLOSCOPE;
        drawOscilloscopeUI();
      }
      // Slider area
      else if (ty >= SLIDER_Y - 20 && ty <= SLIDER_Y + SLIDER_H + 20) {
        int16_t newX = constrain(tx, SLIDER_X, SLIDER_X + SLIDER_W);
        brightness = map(newX, SLIDER_X, SLIDER_X + SLIDER_W, 0, 255);

        analogWrite(LED_PIN, brightness);
        updateBrightnessSlider();

        // Save to NVS
        prefs.begin("nvstore", false);
        prefs.putUChar("bright", brightness);
        prefs.end();
      }
    }
  }

  // Oscilloscope updates
  if (currentScreen == SCREEN_OSCILLOSCOPE) {
    static uint32_t lastUpdate = 0;
    if (micros() - lastUpdate < 8000) return;
    lastUpdate = micros();

    // Read analog value
    uint16_t raw = analogRead(analogPins[currentPin]);
    float voltage = raw * 3.3f / 4095.0f;
    
    // Map to graph coordinates
    uint16_t y = map(raw, 0, 4095, GRAPH_BOTTOM - 2, GRAPH_Y + 2);
    
    // Store in buffer
    graphBuffer[graphIndex] = y;
    
    // Draw the graph
    drawGraph();
    
    // Update reading display
    drawReading(voltage);
    
    // Move to next position
    graphIndex = (graphIndex + 1) % GRAPH_W;
  }
}

//=========================
// GRAPH FUNCTIONS
//=========================
void clearGraph() {
  // Clear graph area
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, C_BG);
  
  // Redraw grid
  drawGrid();
  
  // Reset graph buffer
  for(int i = 0; i < GRAPH_W; i++) {
    graphBuffer[i] = GRAPH_H / 2;
  }
  graphIndex = 0;
}

void drawGraph() {
  uint16_t currentColor = channelColors[currentPin];
  
  // Clear the column we're about to draw over
  tft.drawFastVLine(GRAPH_X + graphIndex, GRAPH_Y, GRAPH_H, C_BG);
  
  // Redraw the grid line at this position
  if ((graphIndex % 40) == 0) {
    tft.drawFastVLine(GRAPH_X + graphIndex, GRAPH_Y, GRAPH_H, C_GRID);
  } else if ((graphIndex % 8) == 0) {
    tft.drawFastVLine(GRAPH_X + graphIndex, GRAPH_Y, GRAPH_H, C_GRID_MAJOR);
  }
  
  // Draw the new line segment
  int prevIndex = (graphIndex == 0) ? GRAPH_W - 1 : graphIndex - 1;
  int x1 = GRAPH_X + prevIndex;
  int x2 = GRAPH_X + graphIndex;
  int y1 = graphBuffer[prevIndex];
  int y2 = graphBuffer[graphIndex];
  
  // Draw line with anti-aliasing effect
  tft.drawLine(x1, y1, x2, y2, currentColor);
  tft.drawLine(x1, y1 + 1, x2, y2 + 1, currentColor);
}

void drawGrid() {
  // Vertical grid lines
  for (int x = 0; x <= 10; x++) {
    int xPos = GRAPH_X + x * (GRAPH_W / 10);
    tft.drawFastVLine(xPos, GRAPH_Y, GRAPH_H, C_GRID);
  }
  
  // Horizontal grid lines (0-3.3V range)
  for (int v = 0; v <= 5; v++) {
    int y = map(v * 819, 0, 4095, GRAPH_BOTTOM, GRAPH_Y);
    tft.drawFastHLine(GRAPH_X, y, GRAPH_W, C_GRID);
    
    // Voltage labels
    tft.setCursor(GRAPH_X - 25, y - 6);
    tft.setTextColor(C_TEXT_DIM);
    tft.setTextSize(1);
    tft.printf("%.1fV", v * 0.66f);
  }
}

//=========================
// UI DRAW FUNCTIONS
//=========================
void drawOscilloscopeUI() {
  // Clear screen
  tft.fillScreen(C_BG);
  
  // Draw header
  drawHeader();
  
  // Draw graph area
  tft.drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, C_GRID);
  
  // Draw grid
  drawGrid();
  
  // Draw footer
  drawFooter();
  
  // Draw channel selector
  drawChannelSelector();
}

void drawHeader() {
  // Header background
  tft.fillRect(0, 0, WIDTH, HEADER_H, C_BG_LIGHT);
  
  // App title
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(2);
  tft.setCursor(15, 15);
  tft.print("OSCILLOSCOPE");
  
  // Settings button (gear icon)
  tft.fillCircle(WIDTH - 30, 25, 15, C_BTN);
  tft.drawCircle(WIDTH - 30, 25, 15, C_ACCENT);
  
  // Draw gear teeth
  for (int i = 0; i < 8; i++) {
    float angle = i * PI / 4;
    int x1 = WIDTH - 30 + 8 * cos(angle);
    int y1 = 25 + 8 * sin(angle);
    int x2 = WIDTH - 30 + 15 * cos(angle);
    int y2 = 25 + 15 * sin(angle);
    tft.drawLine(x1, y1, x2, y2, C_TEXT);
  }
  
  // Gear center
  tft.fillCircle(WIDTH - 30, 25, 4, C_TEXT);
}

void drawFooter() {
  // Footer background
  tft.fillRect(0, HEIGHT - FOOTER_H, WIDTH, FOOTER_H, C_BG_LIGHT);
  
  // Timebase info
  tft.setTextColor(C_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(15, HEIGHT - FOOTER_H + 15);
  tft.print("Timebase: 1ms/div");
  
  // Voltage scale
  tft.setCursor(WIDTH - 100, HEIGHT - FOOTER_H + 15);
  tft.print("Scale: 0.66V/div");
}

void drawChannelSelector() {
  // Clear area
  tft.fillRect(20, 10, 100, 30, C_BG_LIGHT);
  
  // Draw selector
  tft.setTextColor(C_TEXT);
  tft.setTextSize(2);
  tft.setCursor(25, 15);
  tft.print("CH:");
  
  // Draw channel button
  tft.fillRoundRect(70, 12, 45, 26, 5, C_BTN);
  tft.drawRoundRect(70, 12, 45, 26, 5, channelColors[currentPin]);
  
  tft.setTextColor(channelColors[currentPin]);
  tft.setCursor(85, 15);
  tft.print(pinNames[currentPin][3]); // Just the number
}

void drawReading(float voltage) {
  // Clear previous reading area
  tft.fillRect(WIDTH - 120, 15, 100, 20, C_BG_LIGHT);
  
  // Draw voltage reading
  tft.setTextColor(channelColors[currentPin]);
  tft.setTextSize(2);
  tft.setCursor(WIDTH - 115, 15);
  tft.printf("%.3f V", voltage);
}

//=========================
// BRIGHTNESS SCREEN
//=========================
void drawBrightnessUI() {
  // Clear screen
  tft.fillScreen(C_BG);
  
  // Header
  tft.fillRect(0, 0, WIDTH, HEADER_H, C_BG_LIGHT);
  
  // Back button
  tft.fillRoundRect(10, 10, 80, 30, 5, C_BTN);
  tft.drawRoundRect(10, 10, 80, 30, 5, C_ACCENT);
  tft.setTextColor(C_TEXT);
  tft.setTextSize(2);
  tft.setCursor(30, 15);
  tft.print("BACK");
  
  // Title
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(3);
  tft.setCursor(WIDTH/2 - 70, 15);
  tft.print("Brightness");
  
  // Instructions
  tft.setTextColor(C_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(WIDTH/2 - 100, 60);
  tft.print("Drag the slider to adjust display brightness");
  
  updateBrightnessSlider();
}

void updateBrightnessSlider() {
  // Clear slider area
  tft.fillRect(SLIDER_X - 10, SLIDER_Y - 30, SLIDER_W + 20, 80, C_BG);
  
  // Draw slider track
  tft.fillRoundRect(SLIDER_X, SLIDER_Y, SLIDER_W, SLIDER_H, 12, C_SLIDER_BG);
  
  // Draw filled portion
  int fillWidth = map(brightness, 0, 255, 0, SLIDER_W);
  tft.fillRoundRect(SLIDER_X, SLIDER_Y, fillWidth, SLIDER_H, 12, C_SLIDER_FILL);
  
  // Draw slider knob
  int knobX = SLIDER_X + fillWidth - 15;
  knobX = constrain(knobX, SLIDER_X - 15, SLIDER_X + SLIDER_W - 15);
  
  // Clear old knob area completely
  tft.fillRect(knobX - 30, SLIDER_Y - 20, 60, 60, C_BG);
  
  // Draw new knob
  tft.fillCircle(knobX + 15, SLIDER_Y + SLIDER_H/2, 20, C_BTN);
  tft.drawCircle(knobX + 15, SLIDER_Y + SLIDER_H/2, 20, C_ACCENT);
  tft.drawCircle(knobX + 15, SLIDER_Y + SLIDER_H/2, 19, C_ACCENT);
  
  // Draw knob center
  tft.fillCircle(knobX + 15, SLIDER_Y + SLIDER_H/2, 8, C_TEXT);
  
  // Draw percentage
  tft.fillRect(WIDTH/2 - 40, SLIDER_Y - 60, 80, 25, C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextSize(3);
  tft.setCursor(WIDTH/2 - 35, SLIDER_Y - 55);
  tft.printf("%d%%", brightness * 100 / 255);
  
  // Draw min/max labels
  tft.setTextColor(C_TEXT_DIM);
  tft.setTextSize(1);
  tft.setCursor(SLIDER_X - 5, SLIDER_Y + SLIDER_H + 5);
  tft.print("0%");
  tft.setCursor(SLIDER_X + SLIDER_W - 10, SLIDER_Y + SLIDER_H + 5);
  tft.print("100%");
}