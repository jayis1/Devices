/**
 * AsthmaSync — Hub Edge ML (tflite-micro inference)
 * ================================================
 * Runs two models on the ESP32-S3:
 *   1. Wheeze CNN (22-class respiratory sound classifier)
 *   2. Actuation classifier (4-class inhaler actuation)
 *
 * Models are stored in SPIFFS and loaded into PSRAM arena.
 *
 * License: MIT
 */

#include "config.h"
#include "edge_ml.h"
#include "../common/protocol.h"
#include <string.h>
#include <math.h>

/* ── tflite-micro includes ─────────────────────────────── */
/* In production, add tflite-micro as a submodule:
   components/tflite-micro/  (ESP-IDF component) */
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

/* ── Model data (loaded from SPIFFS) ───────────────────── */
static uint8_t *s_wheeze_model_data = NULL;
static uint8_t *s_actuation_model_data = NULL;

/* ── Arena ─────────────────────────────────────────────── */
static uint8_t s_arena[TFLM_ARENA_SIZE] __attribute__((aligned(16)));

/* ── Interpreters ──────────────────────────────────────── */
static tflite::MicroInterpreter *s_wheeze_interp = NULL;
static tflite::MicroInterpreter *s_actuation_interp = NULL;

/* ── Wheeze CNN class labels ───────────────────────────── */
static const char *wheeze_classes[22] = {
    "normal",         /* 0  */
    "wheeze",         /* 1  */
    "stridor",        /* 2  */
    "rhonchi",        /* 3  */
    "crackles_fine",  /* 4  */
    "crackles_coarse",/* 5  */
    "cough",          /* 6  */
    "wheeze_expiratory", /* 7 */
    "wheeze_inspiratory",/* 8 */
    "wheeze_monophonic", /* 9 */
    "wheeze_polyphonic", /* 10 */
    "talking",        /* 11 */
    "speech",         /* 12 */
    "snoring",        /* 13 */
    "grunting",       /* 14 */
    "sneezing",       /* 15 */
    "throat_clearing",/* 16 */
    "yawning",        /* 17 */
    "sighing",        /* 18 */
    "panting",        /* 19 */
    "laughing",       /* 20 */
    "crying",         /* 21 */
};

/* ── Actuation classes ─────────────────────────────────── */
static const char *actuation_classes[4] = {
    "static",        /* 0 — no movement */
    "actuation",     /* 1 — MDI press */
    "pocket_shake",  /* 2 — jostling in pocket */
    "drop",          /* 3 — dropped on floor */
};

/* ── Load model from SPIFFS ────────────────────────────── */
static uint8_t* load_model(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { fclose(f); return NULL; }

    fread(buf, 1, size, f);
    fclose(f);
    return buf;
}

/* ── Initialize edge ML ────────────────────────────────── */
int edge_ml_init(void)
{
    /* Load models */
    s_wheeze_model_data = load_model(WHEEZE_MODEL_PATH);
    s_actuation_model_data = load_model(ACTUATION_MODEL_PATH);

    if (!s_wheeze_model_data || !s_actuation_model_data)
        return -1;

    /* Wheeze interpreter */
    const tflite::Model *wheeze_model = tflite::GetModel(s_wheeze_model_data);
    if (wheeze_model->version() != TFLITE_SCHEMA_VERSION)
        return -2;

    static tflite::MicroMutableOpResolver<10> wheeze_resolver;
    wheeze_resolver.AddConv2D();
    wheeze_resolver.AddDepthwiseConv2D();
    wheeze_resolver.AddFullyConnected();
    wheeze_resolver.AddSoftmax();
    wheeze_resolver.AddReshape();
    wheeze_resolver.AddMaxPool2D();
    wheeze_resolver.AddQuantize();
    wheeze_resolver.AddDequantize();
    wheeze_resolver.AddPad();
    wheeze_resolver.AddMean();

    /* Split arena: 60% wheeze, 40% actuation */
    size_t wheeze_arena_size = TFLM_ARENA_SIZE * 60 / 100;
    static tflite::MicroInterpreter wheeze_interp(
        wheeze_model, wheeze_resolver,
        s_arena, wheeze_arena_size);
    wheeze_interp.AllocateTensors();
    s_wheeze_interp = &wheeze_interp;

    /* Actuation interpreter */
    const tflite::Model *act_model = tflite::GetModel(s_actuation_model_data);
    static tflite::MicroMutableOpResolver<6> act_resolver;
    act_resolver.AddFullyConnected();
    act_resolver.AddSoftmax();
    act_resolver.AddReshape();
    act_resolver.AddQuantize();
    act_resolver.AddDequantize();
    act_resolver.AddMean();

    static tflite::MicroInterpreter act_interp(
        act_model, act_resolver,
        s_arena + wheeze_arena_size,
        TFLM_ARENA_SIZE - wheeze_arena_size);
    act_interp.AllocateTensors();
    s_actuation_interp = &act_interp;

    return 0;
}

/* ── Wheeze classification ─────────────────────────────── */
int edge_ml_classify_wheeze(const uint8_t *mel_spectrogram, size_t mel_len,
                            float *out_probs, int *out_class)
{
    if (!s_wheeze_interp || !mel_spectrogram)
        return -1;

    /* Input: 40 × 32 uint8 mel-spectrogram (quantized) */
    TfLiteTensor *input = s_wheeze_interp->input(0);
    size_t input_size = input->bytes;
    size_t copy_len = mel_len < input_size ? mel_len : input_size;
    memcpy(input->data.uint8, mel_spectrogram, copy_len);

    /* Inference */
    if (s_wheeze_interp->Invoke() != kTfLiteOk)
        return -2;

    /* Output: 22-class softmax (float32 or uint8 depending on model) */
    TfLiteTensor *output = s_wheeze_interp->output(0);
    int num_classes = output->dims->data[output->dims->size - 1];

    int best_class = 0;
    float best_prob = 0.0f;

    if (output->type == kTfLiteFloat32) {
        for (int i = 0; i < num_classes && i < 22; i++) {
            out_probs[i] = output->data.f[i];
            if (out_probs[i] > best_prob) {
                best_prob = out_probs[i];
                best_class = i;
            }
        }
    } else if (output->type == kTfLiteUInt8) {
        float scale = output->params.scale;
        int zero_point = output->params.zero_point;
        for (int i = 0; i < num_classes && i < 22; i++) {
            out_probs[i] = (output->data.uint8[i] - zero_point) * scale;
            if (out_probs[i] > best_prob) {
                best_prob = out_probs[i];
                best_class = i;
            }
        }
    }

    *out_class = best_class;
    return 0;
}

/* ── Actuation classification ──────────────────────────── */
int edge_ml_classify_actuation(const float *accel_features, size_t feat_len,
                               int *out_class, float *out_confidence)
{
    if (!s_actuation_interp || !accel_features)
        return -1;

    TfLiteTensor *input = s_actuation_interp->input(0);
    size_t input_size = input->bytes / sizeof(float);
    size_t copy_len = feat_len < input_size ? feat_len : input_size;
    memcpy(input->data.f, accel_features, copy_len * sizeof(float));

    if (s_actuation_interp->Invoke() != kTfLiteOk)
        return -2;

    TfLiteTensor *output = s_actuation_interp->output(0);
    int num_classes = output->dims->data[output->dims->size - 1];

    int best_class = 0;
    float best_prob = 0.0f;
    for (int i = 0; i < num_classes && i < 4; i++) {
        if (output->type == kTfLiteFloat32) {
            if (output->data.f[i] > best_prob) {
                best_prob = output->data.f[i];
                best_class = i;
            }
        }
    }

    *out_class = best_class;
    *out_confidence = best_prob;
    return 0;
}

/* ── Get class label ───────────────────────────────────── */
const char* edge_ml_wheeze_label(int class_id)
{
    if (class_id < 0 || class_id >= 22)
        return "unknown";
    return wheeze_classes[class_id];
}

const char* edge_ml_actuation_label(int class_id)
{
    if (class_id < 0 || class_id >= 4)
        return "unknown";
    return actuation_classes[class_id];
}

/* ── Alert Zone Calculation ────────────────────────────── */
alert_zone_t edge_ml_calc_zone(const vitals_t *vitals,
                               const air_quality_t *air,
                               uint8_t rescue_use_week)
{
    alert_zone_t zone = ZONE_GREEN;

    /* SpO₂ check */
    if (vitals && vitals->spo2 < THRESH_SPO2_RED)
        zone = ZONE_RED;
    else if (vitals && vitals->spo2 < THRESH_SPO2_YELLOW)
        zone = (zone < ZONE_YELLOW) ? ZONE_YELLOW : zone;

    /* Rescue inhaler use (GINA: >2/week = partly controlled, >4 = uncontrolled) */
    if (rescue_use_week >= THRESH_RESCUE_RED)
        zone = ZONE_RED;
    else if (rescue_use_week >= THRESH_RESCUE_YELLOW)
        zone = (zone < ZONE_YELLOW) ? ZONE_YELLOW : zone;

    /* PM2.5 exposure */
    if (air && air->pm2_5 > THRESH_PM25_HIGH * 10)
        zone = (zone < ZONE_YELLOW) ? ZONE_YELLOW : zone;

    /* VOC exposure */
    if (air && air->voc_index > THRESH_VOC_HIGH)
        zone = (zone < ZONE_YELLOW) ? ZONE_YELLOW : zone;

    return zone;
}