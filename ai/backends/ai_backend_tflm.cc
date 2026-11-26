/*
 * STM32H743 path: TensorFlow Lite Micro backend running the int8 1-D CNN.
 *
 * Build with TFLite Micro sources and, for speed, the CMSIS-NN optimised kernels
 * (OPTIMIZED_KERNEL_DIR=cmsis_nn). The model needs exactly five operators.
 * Verified on the host: TFLite Micro kernels reproduce the reference interpreter bit-exactly
 * on 99.1 % of 4590 test windows, with identical macro-F1 (0.908).
 */
#include <cmath>
#include <cstdint>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

extern "C" {
#include "ai_backend.h"
#include "generated/ai_scaler.h"
}
#include "generated/model_int8_tflite.h"

namespace {
/* Tensor arena. Model activations are a few KB; 16 KB leaves headroom.
 * Measure the real need on target with tflite::RecordingMicroInterpreter and trim. */
constexpr size_t kArenaBytes = 16 * 1024;
alignas(16) uint8_t g_arena[kArenaBytes];

tflite::MicroMutableOpResolver<5> g_resolver;
tflite::MicroInterpreter *g_interp = nullptr;

pal_status_t tflm_init()
{
    const tflite::Model *model = tflite::GetModel(g_ai_model_int8);
    if (model->version() != TFLITE_SCHEMA_VERSION) return PAL_ERR;
    g_resolver.AddConv2D();
    g_resolver.AddFullyConnected();
    g_resolver.AddMean();
    g_resolver.AddReshape();
    g_resolver.AddSoftmax();
    static tflite::MicroInterpreter interp(model, g_resolver, g_arena, kArenaBytes);
    if (interp.AllocateTensors() != kTfLiteOk) return PAL_ERR;
    g_interp = &interp;
    return PAL_OK;
}

pal_status_t tflm_infer(const float *x, float *probs)
{
    if (!g_interp) return PAL_ERR;
    TfLiteTensor *in = g_interp->input(0);
    for (int i = 0; i < AI_WINDOW * AI_N_FEAT; i++) {
        long q = std::lround(x[i] / AI_IN_SCALE) + AI_IN_ZERO;
        in->data.int8[i] = static_cast<int8_t>(q < -128 ? -128 : (q > 127 ? 127 : q));
    }
    if (g_interp->Invoke() != kTfLiteOk) return PAL_ERR;
    const TfLiteTensor *out = g_interp->output(0);
    for (int c = 0; c < AI_N_CLASS; c++)
        probs[c] = (out->data.int8[c] - AI_OUT_ZERO) * AI_OUT_SCALE;
    return PAL_OK;
}

const ai_backend_t kBackend = { "tflite-micro (STM32H743)", tflm_init, tflm_infer };
}  // namespace

extern "C" const ai_backend_t *ai_backend_get(void) { return &kBackend; }
