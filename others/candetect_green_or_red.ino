#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Arduino.h>
#include <ESP32Servo.h>

// Motor pin and channel
int motorFwdPin = 12;
int motorFwdChannel = 4;
int motorSpeed = 200;

// Servo setup
Servo myservo;
int leftPos = 50;
int straightPos = 90;
int rightPos = 130;

// TCS34725 Setup (SDA = IO4, SCL = IO16)
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// Camera configuration
camera_config_t config = {
  .pin_pwdn       = 32,
  .pin_reset      = -1,
  .pin_xclk       = 0,
  .pin_sscb_sda   = 26,
  .pin_sscb_scl   = 27,
  .pin_d7         = 35,
  .pin_d6         = 34,
  .pin_d5         = 39,
  .pin_d4         = 36,
  .pin_d3         = 21,
  .pin_d2         = 19,
  .pin_d1         = 18,
  .pin_d0         = 5,
  .pin_vsync      = 25,
  .pin_href       = 23,
  .pin_pclk       = 22,
  .xclk_freq_hz   = 20000000,
  .ledc_timer     = LEDC_TIMER_0,
  .ledc_channel   = LEDC_CHANNEL_0,
  .pixel_format   = PIXFORMAT_RGB565,
  .frame_size     = FRAMESIZE_QQVGA,
  .jpeg_quality   = 12,
  .fb_count       = 1,
  .grab_mode      = CAMERA_GRAB_LATEST
};

// Movement setup
void robot_setup() {
  ledcSetup(motorFwdChannel, 2000, 8);
  ledcAttachPin(motorFwdPin, motorFwdChannel);
  robot_fwd();
}

void robot_fwd() {
  ledcWrite(motorFwdChannel, motorSpeed);
}

void robot_stop() {
  ledcWrite(motorFwdChannel, 0);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  robot_setup();

  myservo.setPeriodHertz(50);
  myservo.attach(2);
  myservo.write(straightPos);

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera initialized");
}

void loop() {
  robot_fwd();  // Keep motor running

  // === Read color from TCS34725 ===
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);
  Serial.print("TCS R: "); Serial.print(r);
  Serial.print(" G: "); Serial.print(g);
  Serial.print(" B: "); Serial.print(b);
  Serial.print(" C: "); Serial.println(c);

  // Detect blue or orange
  if (b > r * 1.2 && b > g * 1.2) {
    Serial.println("Blue Detected");
  } else if (r > 150 && g > 100 && b < 80) {
    Serial.println("Orange Detected");
  }

  // === Process camera frame ===
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_RGB565) {
    Serial.println("Camera error");
    if (fb) esp_camera_fb_return(fb);
    delay(100);
    return;
  }

  unsigned int redArea = 0;
  unsigned int greenArea = 0;

  for (size_t i = 0; i + 1 < fb->len; i += 2) {
    uint16_t pixel = (fb->buf[i] << 8) | fb->buf[i + 1];
    uint8_t r = (pixel >> 11) & 0x1F;
    uint8_t g = (pixel >> 5) & 0x3F;
    uint8_t b = pixel & 0x1F;

    r = (r * 255) / 31;
    g = (g * 255) / 63;
    b = (b * 255) / 31;

    if (r > 140 && g < 80 && b < 80) redArea++;
    if (g > 50 && r < 40 && b < 40) greenArea++;

    if (i % 1000 == 0) yield();
  }

  esp_camera_fb_return(fb);

  // Decide turn direction
  if (redArea > 50 && redArea > greenArea) {
    Serial.println("Red detected: Turn right");
    myservo.write(rightPos);
    delay(500);
    myservo.write(leftPos);
    delay(500);
    myservo.write(rightPos);
    delay(250);
  } else if (greenArea > 50 && greenArea > redArea) {
    Serial.println("Green detected: Turn left");
    myservo.write(leftPos);
    delay(500);
    myservo.write(rightPos);
    delay(500);
    myservo.write(leftPos);
    delay(250);
  } else {
    Serial.println("No significant color detected: Go straight");
    myservo.write(straightPos);
  }

  delay(1);
}
