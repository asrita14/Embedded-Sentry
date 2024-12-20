#include "mbed.h"
#include <vector>
#include <array>
#include <limits>
#include <cmath>
#include <math.h>
#include "gyro.h"
#include "drivers/LCD_DISCO_F429ZI.h"
#include "drivers/TS_DISCO_F429ZI.h"
#define USER_BUTTON PA_0

// Event flags
#define KEY_FLAG 1
#define UNLOCK_FLAG 2
#define ERASE_FLAG 4
#define DATA_READY_FLAG 8

//LCD font size
#define FONT_SIZE 22

// set limit for unlocking
#define CORRELATION_LIMIT 0.1f

InterruptIn gyro_int2(PA_2, PullDown);
InterruptIn user_button(USER_BUTTON, PullDown);

DigitalOut green_led(LED1);
DigitalOut red_led(LED2);

LCD_DISCO_F429ZI lcd; // LCD object
TS_DISCO_F429ZI ts; // Touch screen object

EventFlags flags; // Event flags

Timer timer; // Timer


// -------------Initializing Functions for data processing, threads, flash and filters--------------

void draw_rounded_button(int x, int y, int width, int height, const char *label);
bool touch_button_validation(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height);

float euclidean_distance(const array<float, 3> &a, const array<float, 3> &b);
float dtwDistance(const vector<array<float, 3>> &s, const vector<array<float, 3>> &t);
void trim_gyro_data(vector<array<float, 3>> &data);
float correlation(const vector<float> &a, const vector<float> &b);
array<float, 3> calculateCorrelation(vector<array<float, 3>>& vec1, vector<array<float, 3>>& vec2);

void gyroscope_thread();
void touch_screen_thread();

bool storeGyroDataToFlash(vector<array<float, 3>> &gesture_key, uint32_t flash_address);
vector<array<float, 3>> readGyroDataFromFlash(uint32_t flash_address, size_t data_size);



//-----------------------------------Callback functions----------------------------------------
void button_press(){ // button press
    flags.set(ERASE_FLAG);
}
void onGyroDataReady(){ // Gyrscope data ready

    flags.set(DATA_READY_FLAG);
}


//--------------------------------------Initialize Global Variables -----------------------------
vector<array<float, 3>> gesture_key; // gesture key
vector<array<float, 3>> unlocking_record; // unlocking record

const int button_x_1 = 60; //record button x axis
const int button_y_1 = 80; // record button y axis
const int button1_width = 120; // record button block width
const int button1_height = 50; // record button block height
const char *button1_label = "RECORD"; //main button label
const int button_x_2 = 60; // unlock button x axis
const int button_y_2 = 180; // unlock button y axis
const int button2_width = 120; // unlock button width
const int button2_height = 50; // ublock button height
const char *button2_label = "UNLOCK"; // main button label
const int title_x = 5; // main title x axis
const int title_y = 30;  // main title y axis
const char *title = "PASSWORD UNLOCKER"; // main title
const int text_x = 5;
const int text_y = 270;
const char *text_0 = "NO PASS RECORDED";
const char *text_1 = "LOCKED";

int err = 0; // debug

/*****************************************************************************
 * @brief main function
 * ***************************************************************************/
int main(){
    lcd.Clear(LCD_COLOR_MAGENTA);

    // Draw 2 touch screen buttons
    draw_rounded_button(button_x_1, button_y_1, button1_width, button1_height, button1_label);
    draw_rounded_button(button_x_2, button_y_2, button2_width, button2_height, button2_label);

    // Display the welcome message
    lcd.DisplayStringAt(title_x, title_y, (uint8_t *)title, CENTER_MODE);

    // initialize all interrupts
    user_button.rise(&button_press);
    gyro_int2.rise(&onGyroDataReady);

    // initialize LEDs
    if (gesture_key.empty()){
        lcd.DisplayStringAt(text_x, text_y, (uint8_t *)text_0, CENTER_MODE);
    }
    else{
        lcd.DisplayStringAt(text_x, text_y, (uint8_t *)text_1, CENTER_MODE);
    }

    // Create the gyroscope thread
    Thread save_key;
    save_key.start(callback(gyroscope_thread));

    // Create the touch screen thread
    Thread touchscreen_thread;
    touchscreen_thread.start(callback(touch_screen_thread));

    // keep main thread alive
    while (1){
        ThisThread::sleep_for(100ms);
    }
}

/**************************************************************************
 * @brief gyroscope gesture key saving thread
 * ***********************************************************************/
void gyroscope_thread(){
    // Add your gyroscope initialization parameters here
    Gyroscope_Init_Parameters gyro_init_param;
    gyro_init_param.conf1 = ODR_200_CUTOFF_50;
    gyro_init_param.conf3 = INT2_DRDY;
    gyro_init_param.conf4 = FULL_SCALE_500;

    // Set up gyroscope's raw data
    Gyroscope_RawData raw_data;

    // initialize a string display_buffer that can be draw on the LCD to dispaly the status
    char display_buffer[50];

    //manually check the signal and set the flag
    // for the first sample.
    if (!(flags.get() & DATA_READY_FLAG) && (gyro_int2.read() == 1)){
        flags.set(DATA_READY_FLAG);
    }

    while (1){
        vector<array<float, 3>> temp_key; // temporary key to store the recording gyro data

        auto flag_check = flags.wait_any(KEY_FLAG | UNLOCK_FLAG | ERASE_FLAG);

        if (flag_check & ERASE_FLAG){
            // Erase the gesture key
            sprintf(display_buffer, "Deleting....");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);               //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                   // text
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            gesture_key.clear();
            
            // Erase the unlocking record
            sprintf(display_buffer, "Pass delete finished.");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);               //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                //text
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            unlocking_record.clear();

            sprintf(display_buffer, "All delete finished.");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);              //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);             //text
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
        }

        if (flag_check & (KEY_FLAG | UNLOCK_FLAG)){
            sprintf(display_buffer, "Pls Wait");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);           //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                //text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

            ThisThread::sleep_for(1s);

            sprintf(display_buffer, "Calibrating...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);            //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                 // text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

            // Initiate gyroscope
            InitiateGyroscope(&gyro_init_param, &raw_data);

            // start recording gesture
            sprintf(display_buffer, "Recording in 3...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);              //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                   // text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);
            sprintf(display_buffer, "Recording in 2...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);           //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                   // text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);
            sprintf(display_buffer, "Recording in 1...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);        //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);            //text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            ThisThread::sleep_for(1s);

            sprintf(display_buffer, "Recording...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);         //bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);          //text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            
            timer.start();
            while (timer.elapsed_time() < 5s){ // gyro data recording loop
                // Wait for the gyroscope data to be ready
                flags.wait_all(DATA_READY_FLAG);
                // Read the data from the gyroscope
                GetCalibratedRawData();
                // Add the converted data to the gesture_key vector
                temp_key.push_back({ConvertDPS(raw_data.x_raw), ConvertDPS(raw_data.y_raw), ConvertDPS(raw_data.z_raw)});
                ThisThread::sleep_for(50ms); // 20Hz
            }
            timer.stop();  // Stop timer
            timer.reset(); // Reset timer

            trim_gyro_data(temp_key);

            sprintf(display_buffer, "Finished...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);    //bg 
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);           // text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
        }

        // check the flag see if it is recording or unlocking
        if (flag_check & KEY_FLAG){
            // if recording finished, and there is no current pass
            if (gesture_key.empty()){
                sprintf(display_buffer, "Saving Pass...");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);   //bg               
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE);  //clear
                lcd.SetTextColor(LCD_COLOR_WHITE);   // text color               
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                // save the key
                gesture_key = temp_key;

                // clear key
                temp_key.clear();

                // confirm the pass saved
                sprintf(display_buffer, "Pass saved...");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);               //bg
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                lcd.SetTextColor(LCD_COLOR_WHITE);                   // text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
            }
            else{
                // if recording finished, and there is a current pass, 
                //remove the old pass and replace with new recording
                sprintf(display_buffer, "Removing old key...");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);               // bg
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                lcd.SetTextColor(LCD_COLOR_WHITE);                  // text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                ThisThread::sleep_for(1s);
                
                // clear old key
                gesture_key.clear();

                // save new key
                gesture_key = temp_key;
                // confirm new pass saved
                sprintf(display_buffer, "New pass is saved.");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);                // bg
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                lcd.SetTextColor(LCD_COLOR_WHITE);                   // text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                // clear temp_key
                temp_key.clear();
            }
        }
        else if (flag_check & UNLOCK_FLAG){
            flags.clear(UNLOCK_FLAG);
            sprintf(display_buffer, "Unlocking...");
            lcd.SetTextColor(LCD_COLOR_MAGENTA);                // bg
            lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
            lcd.SetTextColor(LCD_COLOR_WHITE);                 // text color
            lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

            unlocking_record = temp_key; // save the unlocking record
            temp_key.clear(); // clear temp_key

            // check if the gesture key is empty
            if (gesture_key.empty()){
                sprintf(display_buffer, "NO KEY SAVED.");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);               // bg
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                lcd.SetTextColor(LCD_COLOR_WHITE);                  //text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                unlocking_record.clear(); // clear unlocking record
            }
            else{ // compare the unlock gesture with password
                int unlock = 0; // count for above limit
                array<float, 3> correlationResult = calculateCorrelation(gesture_key, unlocking_record);
                if (err != 0){
                    printf("Error: vectors are not the same size\n");
                }
                else{
                    printf("Correlation values: x = %f, y = %f, z = %f\n", correlationResult[0], correlationResult[1], correlationResult[2]);
                    
                    // check if all values are above limit
                    for (size_t i = 0; i < correlationResult.size(); i++){
                        if (correlationResult[i] > CORRELATION_LIMIT){
                            unlock++;
                        }
                    }
                }

                if (unlock==1){
                    sprintf(display_buffer, "UNLOCK: SUCCESS");
                    lcd.SetTextColor(LCD_COLOR_GREEN);                 // bg 
                    lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                    lcd.SetTextColor(LCD_COLOR_WHITE);                 // text color
                    lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                    // clear
                    unlocking_record.clear();
                    unlock = 0;
                }
                else{
                    sprintf(display_buffer, "UNLOCK: FAILED");
                    lcd.SetTextColor(LCD_COLOR_RED);                 // bg
                    lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                    lcd.SetTextColor(LCD_COLOR_WHITE);                // text color
                    lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);

                    // clear unlocking record
                    unlocking_record.clear();
                    unlock = 0;
                }
            }
        }
        ThisThread::sleep_for(100ms);
    }
}

/********************************************************************
 * @brief touch screen thread
 * *****************************************************************/
void touch_screen_thread(){
    TS_StateTypeDef ts_state;

    if (ts.Init(lcd.GetXSize(), lcd.GetYSize()) != TS_OK){
        printf("error: touch screen failure\r\n");
        return;
    }

    // initialize a string display_buffer that can be draw on the LCD to dispaly the status
    char display_buffer[50];

    while (1){
        ts.GetState(&ts_state);
        if (ts_state.TouchDetected){
            int touch_x = ts_state.X;
            int touch_y = ts_state.Y;

            // Check if the touch is inside record button
            if (touch_button_validation(touch_x, touch_y, button_x_2, button_y_2, button1_width, button1_height)){
                sprintf(display_buffer, "Recording Initiated...");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);                // bg
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                lcd.SetTextColor(LCD_COLOR_WHITE);                   // text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                ThisThread::sleep_for(1s);
                flags.set(KEY_FLAG);
            }

            // Check if the touch is inside unlock button
            if (touch_button_validation(touch_x, touch_y, button_x_1, button_y_1, button2_width, button2_height)){
                sprintf(display_buffer, "Unlocking Initiated...");
                lcd.SetTextColor(LCD_COLOR_MAGENTA);                // bg
                lcd.FillRect(0, text_y, lcd.GetXSize(), FONT_SIZE); // clear
                lcd.SetTextColor(LCD_COLOR_WHITE);                  // text color
                lcd.DisplayStringAt(text_x, text_y, (uint8_t *)display_buffer, CENTER_MODE);
                ThisThread::sleep_for(1s);
                flags.set(UNLOCK_FLAG);
            }
        }
        ThisThread::sleep_for(10ms);
    }
}

/*******************************************************************************
 * @brief store data to flash
 * @param gesture_key: store data
 * @param flash_address: flash address
 * @return true if the data is stored successfully, false otherwise
 * ****************************************************************************/
bool storeGyroDataToFlash(vector<array<float, 3>> &gesture_key, uint32_t flash_address){
    FlashIAP flash;
    flash.init();

    // total size of the data to be stored in bytes
    uint32_t data_size = gesture_key.size() * sizeof(array<float, 3>);

    // Erase the flash sector
    flash.erase(flash_address, data_size);

    // Write the data to flash
    int write_result = flash.program(gesture_key.data(), flash_address, data_size);

    flash.deinit();

    return write_result == 0;
}

/*******************************************************************************
 *
 * @brief read data from flash
 * @param flash_address: the address of the flash
 * @param data_size: the size of the data in bytes
 * @return a vector of array<float, 3> containing the data
 *
 * ****************************************************************************/
vector<array<float, 3>> readGyroDataFromFlash(uint32_t flash_address, size_t data_size){
    vector<array<float, 3>> gesture_key(data_size);

    FlashIAP flash;
    flash.init();

    // Read the data from flash
    flash.read(gesture_key.data(), flash_address, data_size * sizeof(array<float, 3>));

    flash.deinit();

    return gesture_key;
}

/*******************************************************************************
 * @brief draw button with rounded corneers
 * @param x: x coordinate of the button
 * @param y: y coordinate of the button
 * @param width: width of the button
 * @param height: height of the button
 * @param label: label of the button
 * ****************************************************************************/
void draw_rounded_button(int x, int y, int width, int height, const char *label) {
    int radius = 10;  // Radius for the rounded corners

    // Draw the main rectangular body (excluding corners)
    lcd.FillRect(x + radius, y, width - 2 * radius, height);

    // Draw circles at each corner for rounded edges
    lcd.FillCircle(x + radius, y + radius, radius);                     // Top-left
    lcd.FillCircle(x + width - radius, y + radius, radius);             // Top-right
    lcd.FillCircle(x + radius, y + height - radius, radius);            // Bottom-left
    lcd.FillCircle(x + width - radius, y + height - radius, radius);    // Bottom-right

    int font_width = 16; 
    int font_height = 16; 
    int text_width = strlen(label) * font_width;  // Width of the label

    int text_x_position = x + (width - text_width) / 2; // Center horizontally
    int text_y_position = y + (height - font_height) / 2; // Center vertically

    // adjustments for text position
    text_x_position += 12; // Adjust to move text right
    text_y_position -= 1; // Adjust to move text up

    // Set background and text color
    lcd.SetBackColor(LCD_COLOR_BLUE);  // Background color for text
    lcd.SetTextColor(LCD_COLOR_WHITE); // Text color

    // Display the label
    lcd.DisplayStringAt(text_x_position, text_y_position, (uint8_t *)label, LEFT_MODE);
}

/*******************************************************************************
 * @brief Check if the touch point is inside the button
 * @param touch_x: x coordinate of the touch point
 * @param touch_y: y coordinate of the touch point
 * @param button_x: x coordinate of the button
 * @param button_y: y coordinate of the button
 * @param button_width: width of the button
 * @param button_height: height of the button
 * ****************************************************************************/
bool touch_button_validation(int touch_x, int touch_y, int button_x, int button_y, int button_width, int button_height){
    return (touch_x >= button_x && touch_x <= button_x + button_width &&
            touch_y >= button_y && touch_y <= button_y + button_height);
}

/*******************************************************************************
 * @brief Calculate the euclidean distance between two vectors
 * @param vec1: vector 1
 * @param vec2: vector 2
 * @return the euclidean distance between the two vectors
 * ****************************************************************************/
float euclidean_distance(const array<float, 3> &vec1, const array<float, 3> &vec2){
    float sum = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        sum += (vec1[i] - vec2[i]) * (vec1[i] - vec2[i]);
    }
    return sqrt(sum);
}

/*******************************************************************************
 * @brief Calculate the DTW distance between two vectors
 * @param vector1: vector 1
 * @param vector2: vector 2
 * @return the DTW distance between the two vectors
 * ****************************************************************************/
float dtwDistance(const vector<array<float, 3>> &vector1, const vector<array<float, 3>> &vector2){
    vector<vector<float>> dtw_matrix(vector1.size() + 1, vector<float>(vector2.size() + 1, numeric_limits<float>::infinity()));

    dtw_matrix[0][0] = 0;

    for (size_t i = 1; i <= vector1.size(); ++i)
    {
        for (size_t j = 1; j <= vector2.size(); ++j)
        {
            float cost = euclidean_distance(vector1[i - 1], vector2[j - 1]);
            dtw_matrix[i][j] = cost + min({dtw_matrix[i - 1][j], dtw_matrix[i][j - 1], dtw_matrix[i - 1][j - 1]});
        }
    }

    return dtw_matrix[vector1.size()][vector2.size()];
}

/*******************************************************************************
 * @brief Trim the gyro data
 * @param data: the gyro data to trim 
 * ****************************************************************************/
void trim_gyro_data(vector<array<float, 3>> &data){
    float threshold = 0.00001;
    auto ptr = data.begin();
    // find the first element where data from any
    // one direction is larger than the threshold
    while (abs((*ptr)[0]) <= threshold && abs((*ptr)[1]) <= threshold && abs((*ptr)[2]) <= threshold)
    {
        ptr++;
    }
    if (ptr == data.end())
        return;      // all data less than threshold
    auto lptr = ptr; // record the left bound
    // start searching from end to front
    ptr = data.end() - 1;
    while (abs((*ptr)[0]) <= threshold && abs((*ptr)[1]) <= threshold && abs((*ptr)[2]) <= threshold)
    {
        ptr--;
    }
    auto rptr = ptr; // record the right bound
    // start moving elements to the front
    auto replace_ptr = data.begin();
    for (; replace_ptr != lptr && lptr <= rptr; replace_ptr++, lptr++)
    {
        *replace_ptr = *lptr;
    }
    // trim the end
    if (lptr > rptr)
    {
        data.erase(replace_ptr, data.end());
    }
    else
    {
        data.erase(rptr + 1, data.end());
    }
}

/*******************************************************************************
 * @brief Calculate the correlation between two vectors
 * @param vect1: the first vector
 * @param vect2: the second vector
 * @return the correlation between the two vectors
 * ****************************************************************************/
float correlation(const vector<float> &vect1, const vector<float> &vect2){
    // check if the size of the two vectors are the same
    if (vect1.size() != vect2.size())
    {
        err = -1;
        return 0.0f;
    }

    float sum_1 = 0, sum_2 = 0, sum_12 = 0, sq_sum_1 = 0, sq_sum_2 = 0;

    for (size_t i = 0; i < vect1.size(); ++i)
    {
        sum_1 += vect1[i];
        sum_2 += vect2[i];
        sum_12 += vect1[i] * vect2[i];
        sq_sum_1 += vect1[i] * vect1[i];
        sq_sum_2 += vect2[i] * vect2[i];
    }

    size_t n = vect1.size(); // number of elements

    float numerator = n * sum_12 - sum_1 * sum_2; // Covariance
    
    float denominator = sqrt((n * sq_sum_1 - sum_1 * sum_1) * (n * sq_sum_2 - sum_2 * sum_2)); // Standard deviation

    return numerator / denominator;
}

/*******************************************************************************
 * @brief Calculate the correlation between two vectors
 * @param vect1: the first vector
 * @param vect2: the second vector
 * @return the correlation between the two vectors
 * ****************************************************************************/
array<float, 3> calculateCorrelation(vector<array<float, 3>>& vec1, vector<array<float, 3>>& vec2) {
    array<float, 3> result;

    // Calculate the correlation for each coordinate
    for (int i = 0; i < 3; i++) {
        vector<float> vect1;
        vector<float> vect2;

        // Populate 'a' and 'b' with the ith coordinates of vec1 and vec2
        for (const auto& arr : vec1) {
            vect1.push_back(arr[i]);
        }
        for (const auto& arr : vec2) {
            vect2.push_back(arr[i]);
        }

        // Resize 'a' to match the size of 'b', if necessary
        if (vect1.size() > vect2.size()) {
            vect1.resize(vect2.size(), 0);
        } else if (vect2.size() > vect2.size()) {
            vect2.resize(vect1.size(), 0);
        }

        // Calculate the correlation and store the result
        result[i] = correlation(vect1, vect2);
    }

    return result;
}
