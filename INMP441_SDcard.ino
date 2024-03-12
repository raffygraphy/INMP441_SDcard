#include <driver/i2s.h>
#include <FS.h>
#include "SD.h"
#include "SPI.h"
#include "Wav.h"

// I2S microphone and SD card configuration
#define I2S_WS 14
#define I2S_SD 27
#define I2S_SCK 12
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100
#define SAMPLE_BITS 16
#define CHANNELS 1

// Length of audio recording in seconds
const int record_time = 10; // Record for 10 seconds
const char filename[] = "/TEST1.wav";

void setup() {
  Serial.begin(115200);
  delay(5000);

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  
  listSDCardInfo();

  // I2S microphone setup
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Assuming a mono microphone
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

  // Install and start I2S driver
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  // Create a new WAV file
  int headerSize = 44; // WAV header size
  long numSamples = 0;
  int bitsPerSample = 16;
  long sampleRate = SAMPLE_RATE;
  byte header[headerSize];
  File file;

  SD.remove(filename); // Remove a previous version of the file if exists
  file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  writeWAVHeader(file, header, numSamples, sampleRate, bitsPerSample, CHANNELS);
  
  int bufferLen = 64;
  int16_t buffer[bufferLen];

  unsigned long recordingStart = millis();
  while (millis() - recordingStart < record_time * 1000) {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    file.write((const byte*)buffer, bytesRead);
    numSamples += bytesRead / 2;
  }

  file.seek(0); // Go back to the beginning of the file
  writeWAVHeader(file, header, numSamples, sampleRate, bitsPerSample, CHANNELS);
  file.close();

  Serial.println("Recording finished");
}

void loop() {
  // Empty loop
}

void listSDCardInfo() {
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void writeWAVHeader(File file, byte *header, long numSamples, long sampleRate, int bitsPerSample, int channels) {
  CreateWavHeader(header, numSamples * bitsPerSample / 8 * channels);
  file.write(header, 44);
}
