#ifndef INFERENCE_H
#define INFERENCE_H

#define RAIN_FEAT_CNT 10

#define RAIN_FEAT_PRESS_MX 0
#define RAIN_FEAT_PRESS_MN 1
#define RAIN_FEAT_TEMP_DX 2
#define RAIN_FEAT_H_MN 3
#define RAIN_FEAT_TEMP_AUC 4
#define RAIN_FEAT_H_MX 5
#define RAIN_FEAT_DP_MN 6
#define RAIN_FEAT_H_AUC 7
#define RAIN_FEAT_DP_MX 8
#define RAIN_FEAT_TEMP_MX 9

extern float rain_predict(float *feat);

#endif
