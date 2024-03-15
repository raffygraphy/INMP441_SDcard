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
  delay(1000); // Wait for Serial monitor to initialize

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  listSDCardInfo(); // Now it's correctly placed after SD.begin()

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

  esp_err_t i2sDriverInstallStatus = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (i2sDriverInstallStatus != ESP_OK) {
    Serial.println("Failed to install I2S driver");
    return;
  }

  esp_err_t i2sPinConfigStatus = i2s_set_pin(I2S_PORT, &pin_config);
  if (i2sPinConfigStatus != ESP_OK) {
    Serial.println("Failed to set I2S pins");
    return;
  }

  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  long numSamples = 0;
  byte header[44]; // Header size for WAV file is always 44 bytes
  writeWAVHeader(file, header, numSamples, SAMPLE_RATE, SAMPLE_BITS, CHANNELS);
  
  int bufferLen = 64;
  int16_t buffer[bufferLen];

  unsigned long recordingStart = millis();
  while (millis() - recordingStart < record_time * 1000) {
    size_t bytesRead = 0;
    // Reading from I2S
    i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
      file.write((const byte*)buffer, bytesRead);
      numSamples += bytesRead / 2; // 2 bytes per sample (16-bit samples)
    }
  }

  // Rewriting the WAV header now that we know the total numSamples
  file.seek(0);
  writeWAVHeader(file, header, numSamples, SAMPLE_RATE, SAMPLE_BITS, CHANNELS);
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

void writeWAVHeader(File file, byte *header, long numSamples, long sampleRate, int bitsPerSample, int channels) {
  // Calculate the total data size
  // Each sample is bitsPerSample bits, which is bitsPerSample/8 bytes.
  // numSamples is the total number of samples recorded.
  int dataSize = numSamples * (bitsPerSample / 8) * channels;

  // Now call CreateWavHeader with the correct arguments.
  CreateWavHeader(header, dataSize);

  // Write the header to the file
  file.write(header, 44);
}

