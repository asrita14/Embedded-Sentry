#include <mbed.h>

#define WHO_AM_I 0x0F // device id

//control registers 1 - 5
#define CTRL_REG_1 0x20 // 1
#define CTRL_REG_2 0x21 // 2
#define CTRL_REG_3 0x22 // 3
#define CTRL_REG_4 0x23 // 4
#define CTRL_REG_5 0x24 // 5

#define STATUS_REG 0x27 // status register

//angular rate data
#define OUT_X_L 0x28 // X-Low
#define OUT_X_H 0x29 // X-high
#define OUT_Y_L 0x2A // Y-low
#define OUT_Y_H 0x2B // Y-high
#define OUT_Z_L 0x2C // Z-low
#define OUT_Z_H 0x2D // Z-high

//FIFO registers
#define FIFO_CTRL_REG 0x2E // control
#define FIFO_SRC_REG 0x2F  // status 

//Interrupt 1 configuration and thresholds
#define INT1_CFG 0x30 // configuration
#define INT1_SRC 0x31 // source 
#define INT1_TSH_XH 0x32 // X - high
#define INT1_TSH_XL 0x33 // X - low
#define INT1_TSH_YH 0x34 // Y - high
#define INT1_TSH_YL 0x35 // Y - low
#define INT1_TSH_ZH 0x36 // Z - high
#define INT1_TSH_ZL 0x37 // Z - low
#define INT1_DURATION 0x38 // duration

// Output data rate selections and cutoff frequencies
#define ODR_100_CUTOFF_12_5 0x00
#define ODR_100_CUTOFF_25 0x10
#define ODR_200_CUTOFF_12_5 0x40
#define ODR_200_CUTOFF_25 0x50
#define ODR_200_CUTOFF_50 0x60
#define ODR_200_CUTOFF_70 0x70
#define ODR_400_CUTOFF_20 0x80
#define ODR_400_CUTOFF_25 0x90
#define ODR_400_CUTOFF_50 0xa0
#define ODR_400_CUTOFF_110 0xb0
#define ODR_800_CUTOFF_30 0xc0
#define ODR_800_CUTOFF_35 0xd0
#define ODR_800_CUTOFF_50 0xe0
#define ODR_800_CUTOFF_110 0xf0

// High pass filter selections (high pass filter mode disabled)
#define ODR_100_HIGH_PASS_8 0x00
#define ODR_200_HIGH_PASS_15 0x00
#define ODR_400_HIGH_PASS_30 0x00
#define ODR_800_HIGH_PASS_56 0x00

// Interrupts
//      INT1 pin
#define INT1_ENB 0x80 // Interrupt 
#define INT1_BOOT 0x40 // Boot
#define INT1_ACT 0x20 // Interrupt active configuration 
#define INT1_OPEN 0x10 // INT1 pin 
#define INT1_LATCH 0x02 // Latch interrupt request 
//      interrupt generation
#define INT1_ZHIE 0x20 // Z high event
#define INT1_ZLIE 0x10 // Z low event
#define INT1_YHIE 0x08 // Y high event
#define INT1_YLIE 0x04 // Y low event
#define INT1_XHIE 0x02 // X high event
#define INT1_XLIE 0x01 // X low event
#define INT2_DRDY 0x08 // DRDY/INT2

// Fullscale selections
#define FULL_SCALE_245 0x00      // 245 dps
#define FULL_SCALE_500 0x10      // 500 dps
#define FULL_SCALE_2000 0x20     // 2000 dps
#define FULL_SCALE_2000_ALT 0x30 // 2000 dps

// Sensitivities in dps/digit
#define SENSITIVITY_245 0.00875f // 245 dps
#define SENSITIVITY_500 0.0175f  // 500 dps
#define SENSITIVITY_2000 0.07f   // 2000 dps

// Convert constants
#define ARM_MOVE 1              // 
#define DEGREE_TO_RAD 0.0175f // dgree * (pi / 180)

#define POWERON 0x0f  // turn on gyroscope
#define POWEROFF 0x00 // turn off gyroscope

#define SAMPLE_TIME_20 20
#define SAMPLE_INTERVAL_0_05 0.005f

// Initialization parameters
typedef struct
{
    uint8_t conf1;       // output data rate
    uint8_t conf3;       // interrupt config
    uint8_t conf4;       // full sacle selection
} Gyroscope_Init_Parameters;

// Raw data
typedef struct
{
    int16_t x_raw; // X data
    int16_t y_raw; // Y data
    int16_t z_raw; // Z data
} Gyroscope_RawData;

// Calibrated data
typedef struct
{
    int16_t x_calibrated; // X data
    int16_t y_calibrated; // Y data
    int16_t z_calibrated; // Z data
} Gyroscope_CalibratedData;

// Write IO
void WriteIO(uint8_t address, uint8_t data);

// Read IO
void ReadIO(Gyroscope_RawData *rawdata);

// Gyroscope calibration
void GyroscopeCalibration(Gyroscope_RawData *rawdata);

// Gyroscope initialization
void InitiateGyroscope(Gyroscope_Init_Parameters *init_parameters, Gyroscope_RawData *init_raw_data);

// Data conversion: raw -> dps
float ConvertDPS(int16_t rawdata);

// Data conversion: dps -> m/s
float ConvertVelocity(int16_t rawdata);

// Calculate distance from raw data array;
float GetDistance(int16_t arr[]);

// Get calibrated data
void GetCalibratedRawData();

// Turn off the gyroscope
void PowerOff();