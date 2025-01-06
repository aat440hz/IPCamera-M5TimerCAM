#include <M5TimerCAM.h>
#include "battery.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>

#include "camera_pins.h"

const char *ssid     = "SETUP-8CD3";
const char *password = "career6174brace";

void startCameraServer();

// Task for camera operations
void taskCamera(void *pvParameters) {
    while(1) {
        digitalWrite(2, HIGH);
        delay(100);
        digitalWrite(2, LOW);
        delay(100);
        esp_task_wdt_reset(); // Reset watchdog for this task
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 100ms, adjust as needed
    }
}

// Task for WiFi management
void taskWiFi(void *pvParameters) {
    while(1) {
        static unsigned long lastCheck = 0;
        if (millis() - lastCheck > 5000) { // Check WiFi every 5 seconds
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi connection lost. Reconnecting...");
                WiFi.begin(ssid, password);
                while (WiFi.status() != WL_CONNECTED) {
                    delay(500);
                    Serial.print(".");
                }
                Serial.println("");
                Serial.println("WiFi reconnected.");
            }
            lastCheck = millis();
        }
        esp_task_wdt_reset(); // Reset watchdog for this task
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

void setup() {
    Serial.begin(115200);
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // disable brownout detector
    bat_init();
    bat_hold_output();
    Serial.setDebugOutput(true);
    Serial.println();
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 15000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 4;
    config.fb_count     = 2;

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation

    // drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_VGA);

    Serial.printf("Connect to %s, %s\r\n", ssid, password);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    startCameraServer();

    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");

    // Setup watchdog timer
    esp_task_wdt_init(30, true); // 30 seconds timeout
    esp_task_wdt_add(NULL); // Add current task to WDT

    // Create tasks pinned to different cores for better performance
    xTaskCreatePinnedToCore(taskCamera, "CameraTask", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskWiFi, "WiFiTask", 2048, NULL, 1, NULL, 0);
}

void loop() {
    esp_task_wdt_reset(); // Reset watchdog in the main loop
}
