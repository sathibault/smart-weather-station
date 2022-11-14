#include <stdint.h>
#include <math.h>
#include "inference.h"


static float rain_model_theta[3][2+RAIN_FEAT_CNT] = {
  {-0.3712026,-9.6783280,44.3001901,-40.2390818,-23.4438490,51.7049464,30.8883108,-60.5904213,-56.3730851,32.5784757,80.8109603,-7.1629000},
  {0.8481306,13.3487943,24.2217729,-22.9958107,-153.3688573,-10.3758231,212.4496567,40.5237136,-97.9366261,-20.2733559,29.4866741,49.8158486},
  {2.5108855,-1.0063447,2.6905986,-2.9924522,1.3522564,0.3199826,-1.4541124,-1.2390994,0.4676291,1.6111568,2.0534202,-1.4719142}
};

static float rain_model_x_base[RAIN_FEAT_CNT] = {987.420000,986.870000,-7.270000,14.000000,-68.640000,14.000000,1.876111,-220.000000,2.595393,38.220000};
static float rain_model_x_spread[RAIN_FEAT_CNT] = {45.33000,45.84000,15.15000,84.00000,133.98000,84.00000,73.03729,396.00000,72.43178,55.50000};
static float rain_model_y_base = 0;
static float rain_model_y_spread = 1;

static void normalize(float *x, float *base, float *spread, float *res, uint8_t size) {
    for (uint8_t idx = 0; idx < size; idx++)
        res[idx] = 2.0f * (x[idx] - base[idx]) / spread[idx] - 1.0f;
}

static void unnormalize(float *x, float *base, float *spread, float *res, uint8_t size) {
    for (uint8_t idx = 0; idx < size; idx++)
        res[idx] =  spread[idx] * (x[idx] + 1.0f) / 2.0f + base[idx];
}

static float dotprod(float *a, float *b, uint8_t size) {
    float acc = 0.0f;
    for (uint8_t idx = 0; idx < size; idx++)
        acc += a[idx] * b[idx];
    return acc;
}

static void activation(float *x, float *res, uint8_t size) {
    for (uint8_t idx = 0; idx < size; idx++)
        res[idx] =  2.0f / (1.0f + expf(-2.0f*x[idx])) - 1.0f;
}

static float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

float rain_predict(float *feat) {
    uint8_t i;
    float i1[RAIN_FEAT_CNT];
    float o1[3], a1[3];
    float y;

    normalize(feat, rain_model_x_base, rain_model_x_spread, i1, RAIN_FEAT_CNT);

    // Layer 1
    for (i = 0; i < 3; i++)
        o1[i] = dotprod(&rain_model_theta[i][2], i1, RAIN_FEAT_CNT) + rain_model_theta[i][1];
    activation(o1, a1, 3);

    // Layer 2
    y = 0.0f;
    for (i = 0; i < 3; i++)
        y += a1[i] * rain_model_theta[i][0];

    unnormalize(&y, &rain_model_y_base, &rain_model_y_spread, &y, 1);

    return sigmoid(y);
}
