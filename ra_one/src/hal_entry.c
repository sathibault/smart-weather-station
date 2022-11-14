#include <stdio.h>
#include <math.h>
#include "hal_data.h"
#include "inference.h"


void R_BSP_WarmStart(bsp_warm_start_event_t event);

////////// Global measures

float g_temperature, g_pressure;
float g_temperature2, g_humidity;
float g_rain_p;


////////// Measurement window routines

#define MEAS_WINDOW 12

static uint8_t win_count, win_pos;
static float temp_win[MEAS_WINDOW]; // degF
static float press_win[MEAS_WINDOW]; // mbar
static float h_win[MEAS_WINDOW]; // percent
static float dp_win[MEAS_WINDOW]; // degF

static float c_to_f(float temp_c) {
    return 1.8f * temp_c + 32.0f;
}

static float dew_point(float temp_c, float h) {
    float lh = logf(h);
    float DP = 243.04f * (lh + ((17.625f * temp_c) / (243.04f + temp_c))) / (17.625f - lh - ((17.625f * temp_c) / (243.04f + temp_c)));
    return DP;
}

static void push_measures(float temp, float press, float h) {
    temp_win[win_pos] = c_to_f(temp);
    press_win[win_pos] = press;
    h_win[win_pos] = h * 100.0f;
    dp_win[win_pos] = c_to_f(dew_point(temp, h));
    win_pos += 1;
    if (win_pos == MEAS_WINDOW)
        win_pos = 0;
    if (win_count < MEAS_WINDOW)
        win_count += 1;
}


static void build_rain_features(float *feat) {
    uint8_t i, prev, pos;

    float temp_mx = temp_win[win_pos];
    float temp_auc = 0.0f;
    float press_mn = press_win[win_pos];
    float press_mx = press_win[win_pos];
    float H_mn = h_win[win_pos];
    float H_mx = h_win[win_pos];
    float H_auc = 0.0f;
    float DP_mn = dp_win[win_pos];
    float DP_mx = dp_win[win_pos];

    prev = win_pos;
    for (i = 1; i < MEAS_WINDOW; i++) {
        pos = prev + 1;
        if (pos == MEAS_WINDOW)
            pos = 0;

        if (temp_win[pos] > temp_mx)
            temp_mx = temp_win[pos];
        temp_auc += temp_win[pos] - temp_win[win_pos];

        if (press_win[pos] < press_mn)
            press_mn = press_win[pos];
        if (press_win[pos] > press_mx)
            press_mx = press_win[pos];

        if (h_win[pos] < H_mn)
            H_mn = h_win[pos];
        if (h_win[pos] > H_mx)
            H_mx = h_win[pos];
        H_auc += h_win[pos] - h_win[win_pos];

        if (dp_win[pos] < DP_mn)
            DP_mn = dp_win[pos];
        if (dp_win[pos] > DP_mx)
            DP_mx = dp_win[pos];

        prev = pos;
    }

    feat[RAIN_FEAT_TEMP_MX] = temp_mx;
    feat[RAIN_FEAT_TEMP_DX] = temp_win[prev] - temp_win[win_pos]; // last - first (prev is at last value)
    feat[RAIN_FEAT_TEMP_AUC] = temp_auc;

    feat[RAIN_FEAT_PRESS_MX] = press_mx;
    feat[RAIN_FEAT_PRESS_MN] = press_mn;

    feat[RAIN_FEAT_H_MN] = H_mn;
    feat[RAIN_FEAT_H_MX] = H_mx;
    feat[RAIN_FEAT_H_AUC] = H_auc;

    feat[RAIN_FEAT_DP_MN] = DP_mn;
    feat[RAIN_FEAT_DP_MX] = DP_mx;
}

////////// I/O utility routines

void g_comms_i2c_bus0_quick_setup(void);

void g_comms_i2c_bus0_quick_setup(void)
{
    fsp_err_t err;
    i2c_master_instance_t * p_driver_instance = (i2c_master_instance_t *) g_comms_i2c_bus0_extended_cfg.p_driver_instance;

    /* Open I2C driver, this must be done before calling any COMMS API */
    err = p_driver_instance->p_api->open(p_driver_instance->p_ctrl, p_driver_instance->p_cfg);
    assert(FSP_SUCCESS == err);
}

volatile bool g_i2c_completed = false;

void comms_i2c_callback(rm_comms_callback_args_t * p_args)
{
    if (RM_COMMS_EVENT_OPERATION_COMPLETE == p_args->event)
    {
        g_i2c_completed = true;
    } else {
        assert(p_args->event != RM_COMMS_EVENT_ERROR);
    }
}

void g_comms_i2c_device0_quick_setup(void);

void g_comms_i2c_device0_quick_setup(void)
{
    fsp_err_t err;

    err = g_comms_i2c_device0.p_api->open(g_comms_i2c_device0.p_ctrl, g_comms_i2c_device0.p_cfg);
    assert(FSP_SUCCESS == err);
}

void g_comms_i2c_bus1_quick_setup(void);

void g_comms_i2c_bus1_quick_setup(void)
{
    fsp_err_t err;
    i2c_master_instance_t * p_driver_instance = (i2c_master_instance_t *) g_comms_i2c_bus1_extended_cfg.p_driver_instance;

    /* Open I2C driver, this must be done before calling any COMMS API */
    err = p_driver_instance->p_api->open(p_driver_instance->p_ctrl, p_driver_instance->p_cfg);
    assert(FSP_SUCCESS == err);
}

void g_comms_i2c_device1_quick_setup(void);

void g_comms_i2c_device1_quick_setup(void)
{
    fsp_err_t err;

    err = g_comms_i2c_device1.p_api->open(g_comms_i2c_device1.p_ctrl, g_comms_i2c_device1.p_cfg);
    assert(FSP_SUCCESS == err);
}


////////// MS56111 sensor routines

#define MS5611_READ_ADC_CMD       0x00
#define MS5611_READ_PROM_CMD      0xA0
#define MS5611_RESET_CMD          0x1E
#define MS5611_CONVERT_D1_CMD     0x40
#define MS5611_CONVERT_D2_CMD     0x50

float MS6511_cal[7];

void MS5611_command(uint8_t command) {
    fsp_err_t err;

    g_i2c_completed = false;
    err = g_comms_i2c_device0.p_api->write(g_comms_i2c_device0.p_ctrl, &command, 1);
    assert(FSP_SUCCESS == err);

    while (!g_i2c_completed) {}
}

void MS5611_read(uint8_t command, uint8_t *buf, uint8_t size) {
    fsp_err_t err;
    rm_comms_write_read_params_t params = {&command, buf, 1, size};

    g_i2c_completed = false;
    err = g_comms_i2c_device0.p_api->writeRead(g_comms_i2c_device0.p_ctrl, params);
    assert(FSP_SUCCESS == err);

    while (!g_i2c_completed) {}
}

void MS5611_read_init(){
    MS6511_cal[0] = 1;
    //MS6511_cal[1] = 32768.0;          // SENSt1
    //MS6511_cal[2] = 65536.0;          // OFFt1
    //MS6511_cal[3] = 3.90625e-3f;      // TCS
    //MS6511_cal[4] = 7.8125e-3f;       // TCO
    MS6511_cal[1] = 65536L;        //  SENSt1
    MS6511_cal[2] = 131072L;       //  OFFt1
    MS6511_cal[3] = 7.8125E-3;     //  TCS
    MS6511_cal[4] = 1.5625e-2;     //  TCO
    MS6511_cal[5] = 256.0;         // Tref
    MS6511_cal[6] = 1.1920929e-7f; // TEMPSENS

    MS5611_command(MS5611_RESET_CMD);

    // wait 3ms after reset
    R_BSP_SoftwareDelay(3, BSP_DELAY_UNITS_MILLISECONDS);

    for (uint8_t reg = 0; reg < 7; reg++) {
        uint8_t offset = 2 * reg;
        uint8_t buf[2]= {0,0};
        MS5611_read(MS5611_READ_PROM_CMD + offset, buf, 2);
        uint16_t value = buf[0];
        value = (value << 8) + buf[1];
        MS6511_cal[reg] *= value;
    }
}

uint32_t MS5611_convert(uint8_t command, uint8_t bits) {
    uint8_t buf[3];
    uint16_t dly[5] = {600, 1200, 2300, 4600, 9100};

    uint8_t index = bits;
    index -= 8;
    uint8_t offset = index * 2;
    MS5611_command(command + offset);

    R_BSP_SoftwareDelay(dly[index], BSP_DELAY_UNITS_MICROSECONDS);
    MS5611_read(MS5611_READ_ADC_CMD, buf, 3);

    uint32_t value = buf[0];
    value = (value << 8) + buf[1];
    value = (value << 8) + buf[2];

    return value;
}

void MS5611_get_values(float values[2]) {
    uint32_t D1 = MS5611_convert(MS5611_CONVERT_D1_CMD, 12);
    uint32_t D2 = MS5611_convert(MS5611_CONVERT_D2_CMD, 12);

    // TEMP & PRESS MATH - PAGE 7/20
    float dT = (float)D2 - MS6511_cal[5];
    float temperature = 2000 + dT * MS6511_cal[6];

    float offset =  MS6511_cal[2] + dT * MS6511_cal[4];
    float sens = MS6511_cal[1] + dT * MS6511_cal[3];

#if 1
    // SECOND ORDER COMPENSATION - PAGE 8/20
    // COMMENT OUT < 2000 CORRECTION IF NOT NEEDED
    // NOTE TEMPERATURE IS IN 0.01 C
    if (temperature < 2000) {
      float T2 = dT * dT * 4.6566128731E-10f;
      float t = (temperature - 2000) * (temperature - 2000);
      float offset2 = 2.5f * t;
      float sens2 = 1.25f * t;
      // COMMENT OUT < -1500 CORRECTION IF NOT NEEDED
      if (temperature < -1500) {
        t = (temperature + 1500) * (temperature + 1500);
        offset2 += 7 * t;
        sens2 += 5.5f * t;
      }
      temperature -= T2;
      offset -= offset2;
      sens -= sens2;
    }
    // END SECOND ORDER COMPENSATION
#endif

    float pressure = ((float)D1 * sens * 4.76837158205E-7f - offset) * 3.051757813E-5f;

    values[0] = temperature;
    values[1] = pressure;
}


////////// MS56111 sensor routines

void HDC1080_start() {
    fsp_err_t err;
    uint8_t command = 0x00;

    g_i2c_completed = false;
    err = g_comms_i2c_device1.p_api->write(g_comms_i2c_device1.p_ctrl, &command, 1);
    assert(FSP_SUCCESS == err);
    while (!g_i2c_completed) {}

}

void HDC1080_get_values(float values[2]) {
    fsp_err_t err;
    uint8_t buf[4];
    uint16_t word;

    g_i2c_completed = false;
    err = g_comms_i2c_device1.p_api->read(g_comms_i2c_device1.p_ctrl, buf, 4);
    assert(FSP_SUCCESS == err);
    while (!g_i2c_completed) {}

    // temperature conversion
    word = buf[0];
    word = (word << 8) + buf[1];
    values[0] = 165.0f * ((float)word / 65536.0f) - 40.0f;

    // humidity conversion
    word = buf[2];
    word = (word << 8) + buf[3];
    values[1] = (float)word / 65536.0f;
}

////////// Main entry point

void hal_entry (void)
{
    const bsp_delay_units_t bsp_delay_units = BSP_DELAY_UNITS_MILLISECONDS;

    const uint32_t freq_in_hz = 2;
    const uint32_t delay = bsp_delay_units / freq_in_hz;

    g_comms_i2c_bus0_quick_setup();
    g_comms_i2c_device0_quick_setup();

    g_comms_i2c_bus1_quick_setup();
    g_comms_i2c_device1_quick_setup();

    MS5611_read_init();

    float ms5611_out[2];
    float hdc1080_out[2];
    float rain_feat[RAIN_FEAT_CNT];

    while (1) {
        R_BSP_SoftwareDelay(delay, bsp_delay_units);

        // Read two sensors concurrently
        HDC1080_start();

        MS5611_get_values(ms5611_out);
        g_temperature = ms5611_out[0] * 0.01f;
        g_pressure = ms5611_out[1] * 0.01f;
        HDC1080_get_values(hdc1080_out);
        g_temperature2 = hdc1080_out[0];
        g_humidity = hdc1080_out[1];

        push_measures(g_temperature, g_pressure, g_humidity);
        if (win_count == MEAS_WINDOW) {
            build_rain_features(rain_feat);
            g_rain_p = rain_predict(rain_feat);
        }

    }
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart (bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        R_IOPORT_Open(&g_ioport_ctrl, g_ioport.p_cfg);
    }
}
