#include <mbed.h>
#include "gyro.h"

SPI gyroscope(PF_9, PF_8, PF_7); // mosi, miso, sclk
DigitalOut cs(PC_1);

int16_t x_limit; // X calibration limit
int16_t y_limit; // Y calibration limit
int16_t z_limit; // Z calibration limit

int16_t x_zr_sample; // X zero-rate sample
int16_t y_zr_sample; // Y zero-rate sample
int16_t z_zr_sample; // Z zero-rate sample

float sensitivity = 0.0f;

Gyroscope_RawData *gyro_raw;

// Write I/O
void WriteIO(uint8_t address, uint8_t data)
{
  cs = 0;
  gyroscope.write(address);
  gyroscope.write(data);
  cs = 1;
}

// Get raw data from gyroscope
void ReadIO(Gyroscope_RawData *rawdata)
{
  cs = 0;
  gyroscope.write(OUT_X_L | 0x80 | 0x40); // auto-incremented read
  rawdata->x_raw = gyroscope.write(0xff) | gyroscope.write(0xff) << 8;
  rawdata->y_raw = gyroscope.write(0xff) | gyroscope.write(0xff) << 8;
  rawdata->z_raw = gyroscope.write(0xff) | gyroscope.write(0xff) << 8;
  cs = 1;
}

// Calibrate gyroscope before recording
// Find the "turn-on" zero rate level
// Set up thresholds for three axes
// Data below the corresponding threshold will be treated as zero to offset random vibrations when walking
void GyroscopeCalibration(Gyroscope_RawData *rawdata)
{
  int16_t sumX = 0;
  int16_t sumY = 0;
  int16_t sumZ = 0;
  printf("========[Calibrating...]========\r\n");
  for (int i = 0; i < 128; i++)
  {
    ReadIO(rawdata);
    sumX += rawdata->x_raw;
    sumY += rawdata->y_raw;
    sumZ += rawdata->z_raw;
    x_limit = max(x_limit, rawdata->x_raw);
    y_limit = max(y_limit, rawdata->y_raw);
    z_limit = max(z_limit, rawdata->z_raw);
    wait_us(10000);
  }

  x_zr_sample = sumX >> 7; // 128 is 2^7
  y_zr_sample = sumY >> 7;
  z_zr_sample = sumZ >> 7;
  printf("========[Calibration finish.]========\r\n");
}

// Initiate gyroscope, set up control registers
void InitiateGyroscope(Gyroscope_Init_Parameters *init_parameters, Gyroscope_RawData *init_raw_data)
{
  printf("\r\n========[Initializing gyroscope...]========\r\n");
  gyro_raw = init_raw_data;
  cs = 1;
  // set up gyroscope
  gyroscope.format(8, 3);       // 8 bits per SPI frame; polarity 1, phase 0
  gyroscope.frequency(1000000); // clock frequency deafult 1 MHz max:10MHz

  WriteIO(CTRL_REG_1, init_parameters->conf1 | POWERON); // set ODR Bandwidth and enable all 3 axises
  WriteIO(CTRL_REG_3, init_parameters->conf3);           // DRDY enable
  WriteIO(CTRL_REG_4, init_parameters->conf4);           // LSB, full sacle selection: 500dps

  switch (init_parameters->conf4)
  {
  case FULL_SCALE_245:
    sensitivity = SENSITIVITY_245;
    break;

  case FULL_SCALE_500:
    sensitivity = SENSITIVITY_500;
    break;

  case FULL_SCALE_2000:
    sensitivity = SENSITIVITY_2000;
    break;

  case FULL_SCALE_2000_ALT:
    sensitivity = SENSITIVITY_2000;
    break;
  }

  GyroscopeCalibration(gyro_raw); // calibrate the gyroscope and find the threshold for x, y, and z.
  printf("========[Initiation finish.]========\r\n");
}

// convert raw data to dps
float ConvertDPS(int16_t axis_data)
{
  float dps = axis_data * sensitivity;
  return dps;
}

// convert dps to linear velocity
float ConvertVelocity(int16_t axis_data)
{
  float velocity = axis_data * sensitivity * DEGREE_TO_RAD * ARM_MOVE;
  return velocity;
}

// Calculate distance from raw data array;
float GetDistance(int16_t arr[])
{
  float distance = 0.00f;
  for (int i = 0; i < 400; i++)
  {
    float v = ConvertVelocity(arr[i]);
    distance += abs(v * 0.05f);
  }
  return distance;
}

// convert raw data to calibrated data directly
void GetCalibratedRawData()
{
  ReadIO(gyro_raw);

  // offset the zero rate level
  gyro_raw->x_raw -= x_zr_sample;
  gyro_raw->y_raw -= y_zr_sample;
  gyro_raw->z_raw -= z_zr_sample;

  // put data below threshold to zero
  if (abs(gyro_raw->x_raw) < abs(x_limit))
    gyro_raw->x_raw = 0;
  if (abs(gyro_raw->y_raw) < abs(y_limit))
    gyro_raw->y_raw = 0;
  if (abs(gyro_raw->z_raw) < abs(z_limit))
    gyro_raw->z_raw = 0;
}

// turn off the gyroscope
void PowerOff()
{
  WriteIO(CTRL_REG_1, 0x00);
}
