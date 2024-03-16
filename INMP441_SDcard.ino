#include <driver/i2s.h>
#include <FS.h>
#include "SD.h"
#include "SPI.h"
#include "Wav.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display setup
#define TEXT_SIZE 1
#define LINE_HEIGHT 8
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I2S microphone setup
#define I2S_WS 14
#define I2S_SD 27
#define I2S_SCK 12
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100
#define SAMPLE_BITS 16
#define CHANNELS 1

// LED pins
#define RED_LED_PIN 25
#define BLUE_LED_PIN 26

// Button pins
#define BUTTON_DOWN_PIN 2
#define BUTTON_SELECT_PIN 4

// Base filename for recording
String baseFilename = "/rec";
const char extension[] = ".wav";

// Menu options
enum MenuOption { RECORD,
                  VIEW_FILES };
MenuOption selectedOption = RECORD;
bool isRecording = false;

// Setup function
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

  // Initialize button pins
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);

  // Mount SD card
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  // Display SD card information and list files
  listSDCardInfo();
  listFilesOnSDCard();

  // Setup I2S microphone
  I2S_Mic_setup();

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Welcome");

  // Clear display and set text properties
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

// Main loop function
void loop() {

  // Display menu options
  displayMenu();
  // Handle button presses
  handleButtons();

  // Display menu options
  // displayMenu();

  // Handle LED indications
  // handleLEDs();
}

// Function to handle button presses
void handleButtons() {
  static unsigned long lastDebounceTime = 0;
  unsigned long debounceDelay = 200;

  // Handle selection of menu options
  if (!isRecording && digitalRead(BUTTON_DOWN_PIN) == LOW) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      selectedOption = (selectedOption == RECORD) ? VIEW_FILES : RECORD;
      lastDebounceTime = millis();
    }
  }

  // Execute selected option
  if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      executeSelectedOption();
      lastDebounceTime = millis();
    }
  }
}

// Function to execute selected menu option
void executeSelectedOption() {
  switch (selectedOption) {
    case RECORD:
      if (!isRecording) {
        isRecording = true;
        blinkLED(RED_LED_PIN, 100, 400);
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Recording...");
        display.display();
        startRecording();
      } else {
        isRecording = false;
        stopRecording();
        selectedOption = RECORD;
      }
      break;
    case VIEW_FILES:
      viewFilesOnSDCard();
      break;
  }
}

// Function to blink LED
void blinkLED(int pin, int onTime, int offTime) {
  digitalWrite(pin, HIGH);
  delay(onTime);
  digitalWrite(pin, LOW);
  delay(offTime);
}

// Function to display menu options on OLED
void displayMenu() {
  display.clearDisplay();
  display.setCursor(0, 0);

  // Show menu options based on recording status
  if (!isRecording) {
    display.println((selectedOption == RECORD) ? "> Record" : "  Record");
    display.println((selectedOption == VIEW_FILES) ? "> View Files" : "  View Files");
  } else {
    display.println("Recording... Press to save");
  }

  // Display menu on OLED
  display.display();
}

// Function to handle LED indications
void handleLEDs() {
  // Ensure LEDs are off unless specifically activated
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
}

// Function to start recording
void startRecording() {
  isRecording = true;
  String filename = getUniqueFilename(baseFilename, extension);

  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  long numSamples = 0;
  byte header[44];
  writeWAVHeader(file, header, numSamples, SAMPLE_RATE, SAMPLE_BITS, CHANNELS);

  int bufferLen = 64;
  int16_t buffer[bufferLen];

  // Record audio samples while isRecording is true
  while (isRecording) {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
      file.write((const byte *)buffer, bytesRead);
      numSamples += bytesRead / 2;
    }
    if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
      stopRecording();
    }
  }

  // Update WAV header and close file after recording
  file.seek(0);
  writeWAVHeader(file, header, numSamples, SAMPLE_RATE, SAMPLE_BITS, CHANNELS);
  file.close();

  Serial.println("Recording finished");
}

// Function to stop recording
void stopRecording() {
  isRecording = false;
  Serial.println("Recording Stopped, saved.");
  display.clearDisplay();
  blinkLED(BLUE_LED_PIN, 100, 400);
  display.setCursor(0, 0);
  display.println("Saving...");
  display.display();
  delay(500);
}

// Function to generate unique filename
String getUniqueFilename(String baseFilename, const char *extension) {
  String filename = baseFilename + "1" + extension;
  int counter = 1;
  while (SD.exists(filename)) {
    counter++;
    filename = baseFilename + String(counter) + extension;
  }
  return filename;
}

// Function to setup I2S microphone
void I2S_Mic_setup() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  // Install and configure I2S driver
  esp_err_t i2sDriverInstallStatus = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (i2sDriverInstallStatus != ESP_OK) {
    Serial.println("Failed to install I2S driver");
    return;
  }

  // Set I2S pin configuration
  esp_err_t i2sPinConfigStatus = i2s_set_pin(I2S_PORT, &pin_config);
  if (i2sPinConfigStatus != ESP_OK) {
    Serial.println("Failed to set I2S pins");
    return;
  }
}

// Function to list SD card information
void listSDCardInfo() {
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  switch (cardType) {
    case CARD_MMC:
      Serial.println("MMC");
      break;
    case CARD_SD:
      Serial.println("SDSC");
      break;
    case CARD_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("UNKNOWN");
      break;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

// Function to list files on SD card
void listFilesOnSDCard() {
  Serial.println("Files on SD Card:");
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    Serial.print("  ");
    Serial.print(entry.name());
    Serial.print(" - ");
    Serial.print(entry.size());
    Serial.println(" bytes");
    entry.close();
  }
}

// Function to view files on SD card and display on OLED
void viewFilesOnSDCards() {
  Serial.println("Files on SD Card:");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Files on SD Card:");

  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    display.print("  ");
    display.print(entry.name());
    display.print(" - ");
    display.print(entry.size());
    // display.println(" B"); BYTES
    entry.close();
  }
  display.display();
  delay(3000);
}


// Function to view files on SD card and display on OLED
void viewFilesOnSDCard() {
  bool viewLoop = true;
  Serial.println("Files on SD Card:");
  display.clearDisplay();
  display.setTextSize(TEXT_SIZE);
  
  File root = SD.open("/");
  while (viewLoop) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Files on SD Card:");
    
    int y = LINE_HEIGHT; // Start printing files below the header
    root.rewindDirectory(); // Start reading files from the beginning
    while (true) {
      File entry = root.openNextFile();
      if (!entry) {
        break;
      }
      // Print file name
      display.setCursor(0, y);
      display.print(entry.name());
      y += LINE_HEIGHT;
      // Check if the display is full
      if (y >= SCREEN_HEIGHT - LINE_HEIGHT) {
        y = LINE_HEIGHT; // Reset y position for scrolling
        display.display(); // Display content so far
        delay(1500); // Delay to allow the user to read before scrolling
        display.clearDisplay(); // Clear the display for next content
      }
      entry.close();
    }
    display.display(); // Display the remaining content
    delay(5000); // Delay to keep the files displayed for 5 seconds before scrolling

    if(digitalRead(BUTTON_DOWN_PIN) == LOW) {
      viewLoop = false;
    }
  }
}


// Function to write WAV header
void writeWAVHeader(File file, byte *header, long numSamples, long sampleRate, int bitsPerSample, int channels) {
  int dataSize = numSamples * (bitsPerSample / 8) * channels;
  CreateWavHeader(header, dataSize);
  file.write(header, 44);
}
