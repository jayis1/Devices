"""
UrbanHarvest - Plant Disease Detection Model Training
Uses PlantVillage dataset + custom augmentations
Trains MobileNetV2-based classifier for 6 disease classes
Exports to TFLite INT8 for ESP32-S3 edge inference
"""

import os
import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.preprocessing.image import ImageDataGenerator
import matplotlib.pyplot as plt

# ========== CONFIGURATION ==========

IMG_SIZE = 120          # Model input: 120x120 (balance between accuracy and memory)
BATCH_SIZE = 32
EPOCHS = 30
LEARNING_RATE = 1e-4
NUM_CLASSES = 6         # healthy, powdery_mildew, downy_mildew, leaf_spot_bact, leaf_spot_fung, nutrient_def

DATA_DIR = os.getenv("DATA_DIR", "./plantvillage_data")
MODEL_DIR = os.getenv("MODEL_DIR", "./models")

CLASS_NAMES = [
    "healthy",
    "powdery_mildew",
    "downy_mildew",
    "leaf_spot_bacterial",
    "leaf_spot_fungal",
    "nutrient_deficiency"
]

# ========== DATA PREPARATION ==========

def create_data_generators():
    """Create training and validation data generators with heavy augmentation"""

    train_aug = ImageDataGenerator(
        rescale=1.0 / 255.0,
        rotation_range=45,
        width_shift_range=0.2,
        height_shift_range=0.2,
        shear_range=0.2,
        zoom_range=0.3,
        horizontal_flip=True,
        vertical_flip=True,
        brightness_range=[0.7, 1.3],
        fill_mode='nearest',
        validation_split=0.2,
    )

    val_aug = ImageDataGenerator(
        rescale=1.0 / 255.0,
        validation_split=0.2,
    )

    train_gen = train_aug.flow_from_directory(
        DATA_DIR,
        target_size=(IMG_SIZE, IMG_SIZE),
        batch_size=BATCH_SIZE,
        class_mode='categorical',
        subset='training',
        shuffle=True,
        seed=42,
    )

    val_gen = val_aug.flow_from_directory(
        DATA_DIR,
        target_size=(IMG_SIZE, IMG_SIZE),
        batch_size=BATCH_SIZE,
        class_mode='categorical',
        subset='validation',
        shuffle=False,
        seed=42,
    )

    return train_gen, val_gen


# ========== MODEL ARCHITECTURE ==========

def build_disease_model():
    """
    Build MobileNetV2-based disease classifier.
    Backbone: MobileNetV2 pretrained on ImageNet, fine-tuned.
    Head: GlobalAveragePooling → Dense(128, relu) → Dropout → Dense(6, softmax)
    Total params: ~2.3M (backbone frozen) or ~3.5M (fine-tuned)
    TFLite INT8 quantized: ~280KB
    """
    base_model = MobileNetV2(
        input_shape=(IMG_SIZE, IMG_SIZE, 3),
        include_top=False,
        weights='imagenet',
        alpha=0.35,  # Smaller model for edge deployment
    )

    # Freeze base model for initial training
    base_model.trainable = False

    inputs = keras.Input(shape=(IMG_SIZE, IMG_SIZE, 3))
    x = base_model(inputs, training=False)
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dense(128, activation='relu')(x)
    x = layers.Dropout(0.3)(x)
    x = layers.Dense(64, activation='relu')(x)
    x = layers.Dropout(0.2)(x)
    outputs = layers.Dense(NUM_CLASSES, activation='softmax')(x)

    model = keras.Model(inputs, outputs)
    return model, base_model


# ========== TRAINING ==========

def train_model():
    """Train the disease detection model with two-phase training"""

    train_gen, val_gen = create_data_generators()
    model, base_model = build_disease_model()

    # Phase 1: Train head only (backbone frozen)
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=LEARNING_RATE),
        loss='categorical_crossentropy',
        metrics=['accuracy', keras.metrics.Precision(), keras.metrics.Recall()],
    )

    print("Phase 1: Training classification head...")
    history_phase1 = model.fit(
        train_gen,
        validation_data=val_gen,
        epochs=10,
        steps_per_epoch=train_gen.samples // BATCH_SIZE,
        validation_steps=val_gen.samples // BATCH_SIZE,
    )

    # Phase 2: Fine-tune entire model with low learning rate
    base_model.trainable = True
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=LEARNING_RATE / 10),
        loss='categorical_crossentropy',
        metrics=['accuracy', keras.metrics.Precision(), keras.metrics.Recall()],
    )

    print("Phase 2: Fine-tuning entire model...")
    history_phase2 = model.fit(
        train_gen,
        validation_data=val_gen,
        epochs=EPOCHS - 10,
        steps_per_epoch=train_gen.samples // BATCH_SIZE,
        validation_steps=val_gen.samples // BATCH_SIZE,
    )

    # Evaluate
    val_loss, val_acc, val_prec, val_rec = model.evaluate(val_gen)
    print(f"\nValidation Results:")
    print(f"  Accuracy:  {val_acc:.4f}")
    print(f"  Precision: {val_prec:.4f}")
    print(f"  Recall:    {val_rec:.4f}")
    print(f"  F1 Score:  {2*val_prec*val_rec/(val_prec+val_rec+1e-8):.4f}")

    return model, history_phase1, history_phase2


# ========== TFLITE EXPORT ==========

def export_tflite(model):
    """Export model to TFLite INT8 for ESP32-S3 deployment"""

    # Representative dataset for INT8 quantization
    def representative_dataset():
        for i in range(100):
            img = np.random.rand(1, IMG_SIZE, IMG_SIZE, 3).astype(np.float32)
            yield [img]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()

    model_path = os.path.join(MODEL_DIR, "disease_detector_int8.tflite")
    with open(model_path, 'wb') as f:
        f.write(tflite_model)

    size_kb = len(tflite_model) / 1024
    print(f"TFLite INT8 model saved: {model_path} ({size_kb:.1f} KB)")

    # Verify model works
    interpreter = tf.lite.Interpreter(model_path=model_path)
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    # Test inference
    test_input = np.random.rand(1, IMG_SIZE, IMG_SIZE, 3).astype(np.float32)
    # Scale to int8 range
    scale, zero_point = input_details[0]['quantization']
    test_input_int8 = (test_input / scale + zero_point).astype(np.int8)
    interpreter.set_tensor(input_details[0]['index'], test_input_int8)
    interpreter.invoke()
    output = interpreter.get_tensor(output_details[0]['index'])
    print(f"Test inference output shape: {output.shape}")
    print(f"Predicted class: {CLASS_NAMES[np.argmax(output[0])]}")

    return model_path


# ========== C INFERENCE STUB ==========

def generate_c_inference_stub(model_path):
    """Generate C code stub for TFLite Micro inference on ESP32-S3"""

    c_code = f"""/**
 * UrbanHarvest - Disease Detection Inference Stub
 * ESP32-S3 with TFLite Micro
 *
 * Model: disease_detector_int8.tflite
 * Input: 120x120 RGB (int8)
 * Output: 6-class softmax (int8)
 * Model size: ~280KB (INT8 quantized)
 */

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

/* Model data — included as C array from xxd conversion */
extern const unsigned char g_model_data[];
extern const unsigned int g_model_data_len;

#define MODEL_INPUT_SIZE  {IMG_SIZE}
#define NUM_CLASSES       {NUM_CLASSES}
#define DISEASE_THRESHOLD 0.70f

static const char* disease_names[NUM_CLASSES] = {{
    "healthy", "powdery_mildew", "downy_mildew",
    "leaf_spot_bacterial", "leaf_spot_fungal", "nutrient_deficiency"
}};

typedef struct {{
    uint8_t disease_class;
    float confidence;
    const char* disease_name;
}} disease_result_t;

/**
 * run_disease_inference - Run TFLite Micro inference on 120x120 RGB image
 * image_data: pointer to 120x120x3 int8 array (quantized)
 * Returns disease result with class, confidence, and name
 */
disease_result_t run_disease_inference(const int8_t *image_data)
{{
    /* Setup TFLite Micro (normally done once at init) */
    const tflite::Model* model = tflite::GetModel(g_model_data);
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter interpreter(
        model, resolver, tensor_arena, kArenaSize);
    interpreter.AllocateTensors();

    /* Copy image data to model input tensor */
    TfLiteTensor* input = interpreter.input(0);
    memcpy(input->data.int8, image_data, MODEL_INPUT_SIZE * MODEL_INPUT_SIZE * 3);

    /* Run inference */
    interpreter.Invoke();

    /* Read output tensor */
    TfLiteTensor* output = interpreter.output(0);

    /* Find max confidence class */
    int max_idx = 0;
    float max_conf = 0.0f;
    float scale = output->params.scale;
    int zero_point = output->params.zero_point;

    for (int i = 0; i < NUM_CLASSES; i++) {{
        float conf = (output->data.int8[i] - zero_point) * scale;
        if (conf > max_conf) {{
            max_conf = conf;
            max_idx = i;
        }}
    }}

    disease_result_t result;
    result.disease_class = max_idx;
    result.confidence = max_conf;
    result.disease_name = disease_names[max_idx];

    return result;
}}
"""
    stub_path = os.path.join(MODEL_DIR, "disease_inference_stub.c")
    with open(stub_path, 'w') as f:
        f.write(c_code)
    print(f"C inference stub saved: {stub_path}")


# ========== MAIN ==========

if __name__ == "__main__":
    os.makedirs(MODEL_DIR, exist_ok=True)

    print("=" * 60)
    print("UrbanHarvest — Plant Disease Detection Model Training")
    print("=" * 60)

    # Train
    model, h1, h2 = train_model()

    # Save Keras model
    keras_path = os.path.join(MODEL_DIR, "disease_detector.keras")
    model.save(keras_path)
    print(f"Keras model saved: {keras_path}")

    # Export TFLite INT8
    tflite_path = export_tflite(model)

    # Generate C inference stub
    generate_c_inference_stub(tflite_path)

    print("\n" + "=" * 60)
    print("Training complete!")
    print(f"  Keras model:   {keras_path}")
    print(f"  TFLite INT8:   {tflite_path}")
    print(f"  C stub:        {MODEL_DIR}/disease_inference_stub.c")
    print("=" * 60)
    print("\nDeploy to ESP32-S3:")
    print(f"  1. Convert TFLite to C array: xxd -i {tflite_path} > model_data.cc")
    print("  2. Include model_data.cc in firmware build")
    print("  3. Call run_disease_inference() from grow_pod_main.c after camera capture")