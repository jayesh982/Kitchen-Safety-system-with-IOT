"""
Smart Kitchen Safety System - Program 5
Platform: TinyML (Google Colab / Python)
Role: Kitchen Sound Event Classifier

What it does:
  - Generates synthetic kitchen audio data (simulating real recordings)
  - Extracts audio features (MFCCs - Mel Frequency Cepstral Coefficients)
  - Trains a small neural network to classify kitchen sounds
  - Classes: Normal, Boiling Over, Gas Hissing, Smoke Alarm, Glass Breaking
  - Converts model to TensorFlow Lite for microcontroller deployment
  - Tests the model with sample predictions

How to run:
  1. Open Google Colab (colab.research.google.com)
  2. Create new notebook
  3. Copy-paste each cell (separated by # === CELL === comments)
  4. Run cells in order (Shift+Enter)

No hardware needed - runs 100% in browser!
"""

# === CELL 1: Install Dependencies ===
# Run this cell first in Google Colab
"""
!pip install tensorflow numpy matplotlib librosa scikit-learn
"""

# === CELL 2: Imports ===
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
import tensorflow as tf
from tensorflow import keras
import warnings
warnings.filterwarnings('ignore')

print("TensorFlow version:", tf.__version__)
print("Kitchen Sound Classifier - TinyML")
print("=" * 40)

# === CELL 3: Generate Synthetic Kitchen Audio Data ===
"""
In a real project, you would record actual kitchen sounds.
Here we simulate audio features (MFCCs) for each sound class
to demonstrate the ML pipeline.
"""

np.random.seed(42)
SAMPLE_RATE = 16000
N_MFCC = 13           # Number of MFCC features
N_FRAMES = 32         # Time frames per sample
SAMPLES_PER_CLASS = 200

# Sound classes
CLASSES = ['normal', 'boiling_over', 'gas_hissing', 'smoke_alarm', 'glass_breaking']

def generate_synthetic_mfcc(sound_class, n_samples):
    """
    Generate synthetic MFCC-like features for each sound class.
    Each class has distinct frequency/energy patterns:
      - Normal: low energy, flat spectrum
      - Boiling over: mid-freq bubbling pattern, periodic
      - Gas hissing: high-freq noise, steady energy
      - Smoke alarm: strong periodic peaks at specific freq
      - Glass breaking: broadband burst, high energy transient
    """
    data = []

    for _ in range(n_samples):
        if sound_class == 'normal':
            # Low energy, gentle variation
            mfcc = np.random.normal(loc=-5, scale=2, size=(N_FRAMES, N_MFCC))
            mfcc[:, 0] = np.random.normal(-10, 1, N_FRAMES)  # low energy

        elif sound_class == 'boiling_over':
            # Bubbling: periodic mid-frequency energy
            mfcc = np.random.normal(loc=0, scale=3, size=(N_FRAMES, N_MFCC))
            # Add bubbling periodicity
            bubble = np.sin(np.linspace(0, 8 * np.pi, N_FRAMES)) * 5
            mfcc[:, 1] += bubble
            mfcc[:, 2] += bubble * 0.7
            mfcc[:, 0] = np.random.normal(5, 2, N_FRAMES)

        elif sound_class == 'gas_hissing':
            # High frequency steady noise
            mfcc = np.random.normal(loc=3, scale=1, size=(N_FRAMES, N_MFCC))
            # Strong high-frequency components
            mfcc[:, 8:] += np.random.normal(8, 1, (N_FRAMES, N_MFCC - 8))
            mfcc[:, 0] = np.random.normal(3, 0.5, N_FRAMES)  # steady energy

        elif sound_class == 'smoke_alarm':
            # Strong periodic beeping at specific frequency
            mfcc = np.random.normal(loc=2, scale=2, size=(N_FRAMES, N_MFCC))
            # Beeping pattern: on-off-on-off
            beep = np.zeros(N_FRAMES)
            for i in range(N_FRAMES):
                if (i // 4) % 2 == 0:
                    beep[i] = 15
            mfcc[:, 3] += beep
            mfcc[:, 4] += beep * 0.8
            mfcc[:, 0] = beep * 0.5 + np.random.normal(2, 1, N_FRAMES)

        elif sound_class == 'glass_breaking':
            # Sharp transient burst then decay
            mfcc = np.random.normal(loc=1, scale=4, size=(N_FRAMES, N_MFCC))
            # Impact at start, then ringing decay
            impact = np.exp(-np.linspace(0, 5, N_FRAMES)) * 20
            mfcc[:, :] += impact.reshape(-1, 1)
            # Broadband: all frequencies active
            mfcc[:8, 5:] += np.random.normal(10, 2, (8, N_MFCC - 5))
            mfcc[:, 0] = impact + np.random.normal(0, 1, N_FRAMES)

        # Add slight random noise for variety
        mfcc += np.random.normal(0, 0.5, (N_FRAMES, N_MFCC))
        data.append(mfcc)

    return np.array(data)


# Generate dataset
print("Generating synthetic kitchen audio data...")
print(f"Classes: {CLASSES}")
print(f"Samples per class: {SAMPLES_PER_CLASS}")
print(f"Feature shape: {N_FRAMES} frames x {N_MFCC} MFCCs")
print()

X_all = []
y_all = []

for cls in CLASSES:
    samples = generate_synthetic_mfcc(cls, SAMPLES_PER_CLASS)
    X_all.append(samples)
    y_all.extend([cls] * SAMPLES_PER_CLASS)
    print(f"  Generated {SAMPLES_PER_CLASS} samples for '{cls}'")

X = np.concatenate(X_all, axis=0)
y = np.array(y_all)

# Encode labels
label_encoder = LabelEncoder()
y_encoded = label_encoder.fit_transform(y)
y_onehot = keras.utils.to_categorical(y_encoded, num_classes=len(CLASSES))

# Split dataset
X_train, X_test, y_train, y_test = train_test_split(
    X, y_onehot, test_size=0.2, random_state=42, stratify=y_encoded
)

print(f"\nDataset ready:")
print(f"  Training: {X_train.shape[0]} samples")
print(f"  Testing:  {X_test.shape[0]} samples")
print(f"  Input shape: {X_train.shape[1:]}")


# === CELL 4: Visualize Audio Features ===
fig, axes = plt.subplots(1, 5, figsize=(20, 4))
fig.suptitle('MFCC Features for Each Kitchen Sound Class', fontsize=14)

for idx, cls in enumerate(CLASSES):
    # Get first sample of this class
    sample_idx = idx * SAMPLES_PER_CLASS
    axes[idx].imshow(X[sample_idx].T, aspect='auto', origin='lower', cmap='viridis')
    axes[idx].set_title(cls.replace('_', ' ').title())
    axes[idx].set_xlabel('Time Frame')
    axes[idx].set_ylabel('MFCC Coefficient')

plt.tight_layout()
plt.savefig('mfcc_features.png', dpi=100, bbox_inches='tight')
plt.show()
print("Each class has a visually distinct MFCC pattern!")


# === CELL 5: Build TinyML Model ===
"""
Small model designed to run on microcontrollers:
  - ~5,000 parameters (fits in < 20KB RAM)
  - Uses Conv1D for temporal pattern detection
  - Fast inference (~1ms on ESP32)
"""

model = keras.Sequential([
    # Input: (32 frames, 13 MFCCs)
    keras.layers.Input(shape=(N_FRAMES, N_MFCC)),

    # Conv layer: detect local patterns in time
    keras.layers.Conv1D(16, kernel_size=3, activation='relu', padding='same'),
    keras.layers.MaxPooling1D(pool_size=2),

    # Second conv layer
    keras.layers.Conv1D(8, kernel_size=3, activation='relu', padding='same'),
    keras.layers.MaxPooling1D(pool_size=2),

    # Flatten and classify
    keras.layers.Flatten(),
    keras.layers.Dense(16, activation='relu'),
    keras.layers.Dropout(0.3),
    keras.layers.Dense(len(CLASSES), activation='softmax')
])

model.compile(
    optimizer='adam',
    loss='categorical_crossentropy',
    metrics=['accuracy']
)

print("Model Architecture:")
model.summary()

# Count parameters
total_params = model.count_params()
model_size_kb = total_params * 4 / 1024  # float32 = 4 bytes
print(f"\nTotal parameters: {total_params}")
print(f"Estimated model size: {model_size_kb:.1f} KB")
print("(Small enough for ESP32 / Arduino Nano 33 BLE / Pico!)")


# === CELL 6: Train the Model ===
print("\nTraining the kitchen sound classifier...")
print("=" * 40)

history = model.fit(
    X_train, y_train,
    validation_split=0.2,
    epochs=30,
    batch_size=32,
    verbose=1
)

# Plot training history
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

ax1.plot(history.history['accuracy'], label='Train Accuracy')
ax1.plot(history.history['val_accuracy'], label='Validation Accuracy')
ax1.set_title('Model Accuracy')
ax1.set_xlabel('Epoch')
ax1.set_ylabel('Accuracy')
ax1.legend()
ax1.grid(True)

ax2.plot(history.history['loss'], label='Train Loss')
ax2.plot(history.history['val_loss'], label='Validation Loss')
ax2.set_title('Model Loss')
ax2.set_xlabel('Epoch')
ax2.set_ylabel('Loss')
ax2.legend()
ax2.grid(True)

plt.tight_layout()
plt.savefig('training_history.png', dpi=100, bbox_inches='tight')
plt.show()


# === CELL 7: Evaluate Model ===
print("\nEvaluating on test set...")
test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
print(f"Test Accuracy: {test_accuracy * 100:.1f}%")
print(f"Test Loss: {test_loss:.4f}")

# Confusion matrix
from sklearn.metrics import confusion_matrix, classification_report

y_pred = model.predict(X_test, verbose=0)
y_pred_classes = np.argmax(y_pred, axis=1)
y_true_classes = np.argmax(y_test, axis=1)

print("\nClassification Report:")
print(classification_report(
    y_true_classes, y_pred_classes,
    target_names=CLASSES
))

# Plot confusion matrix
cm = confusion_matrix(y_true_classes, y_pred_classes)
fig, ax = plt.subplots(figsize=(8, 6))
im = ax.imshow(cm, interpolation='nearest', cmap='Blues')
ax.figure.colorbar(im, ax=ax)
ax.set(
    xticks=np.arange(len(CLASSES)),
    yticks=np.arange(len(CLASSES)),
    xticklabels=[c.replace('_', '\n') for c in CLASSES],
    yticklabels=[c.replace('_', '\n') for c in CLASSES],
    title='Confusion Matrix',
    xlabel='Predicted',
    ylabel='Actual'
)
for i in range(len(CLASSES)):
    for j in range(len(CLASSES)):
        ax.text(j, i, str(cm[i, j]), ha='center', va='center',
                color='white' if cm[i, j] > cm.max() / 2 else 'black')

plt.tight_layout()
plt.savefig('confusion_matrix.png', dpi=100, bbox_inches='tight')
plt.show()


# === CELL 8: Convert to TensorFlow Lite (for Microcontroller) ===
print("\nConverting to TensorFlow Lite...")

# Standard TFLite conversion
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

tflite_size = len(tflite_model) / 1024
print(f"TFLite model size: {tflite_size:.1f} KB")

# Quantized version (even smaller, for microcontrollers)
converter_quant = tf.lite.TFLiteConverter.from_keras_model(model)
converter_quant.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model_quant = converter_quant.convert()

tflite_quant_size = len(tflite_model_quant) / 1024
print(f"Quantized TFLite model size: {tflite_quant_size:.1f} KB")
print(f"Size reduction: {(1 - tflite_quant_size / tflite_size) * 100:.0f}%")

# Save models
with open('kitchen_sound_model.tflite', 'wb') as f:
    f.write(tflite_model)

with open('kitchen_sound_model_quant.tflite', 'wb') as f:
    f.write(tflite_model_quant)

print("\nModels saved:")
print("  kitchen_sound_model.tflite (standard)")
print("  kitchen_sound_model_quant.tflite (quantized - for MCU)")


# === CELL 9: Test Live Predictions ===
print("\n" + "=" * 50)
print("LIVE PREDICTION DEMO")
print("=" * 50)

# Run TFLite interpreter
interpreter = tf.lite.Interpreter(model_content=tflite_model_quant)
interpreter.allocate_tensors()
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

def predict_sound(mfcc_features):
    """Predict kitchen sound class from MFCC features"""
    input_data = np.expand_dims(mfcc_features, axis=0).astype(np.float32)
    interpreter.set_tensor(input_details[0]['index'], input_data)
    interpreter.invoke()
    output = interpreter.get_tensor(output_details[0]['index'])[0]
    return output

# Test with one sample from each class
print("\nTesting with one sample from each class:\n")
print(f"{'Sound':>20} | {'Predicted':>20} | {'Confidence':>10} | {'Result':>8}")
print("-" * 68)

correct = 0
for idx, cls in enumerate(CLASSES):
    # Generate a fresh test sample
    test_sample = generate_synthetic_mfcc(cls, 1)[0]
    probs = predict_sound(test_sample)
    pred_idx = np.argmax(probs)
    pred_class = CLASSES[pred_idx]
    confidence = probs[pred_idx] * 100

    is_correct = pred_class == cls
    if is_correct:
        correct += 1

    print(f"{cls:>20} | {pred_class:>20} | {confidence:>8.1f}% | {'OK' if is_correct else 'MISS':>8}")

print(f"\nDemo accuracy: {correct}/{len(CLASSES)} correct")


# === CELL 10: Generate C Header for Microcontroller Deployment ===
print("\n" + "=" * 50)
print("MICROCONTROLLER DEPLOYMENT")
print("=" * 50)

def convert_to_c_array(tflite_model, output_name="kitchen_model"):
    """Convert TFLite model to C header file for Arduino/ESP32"""
    c_array = ", ".join([f"0x{b:02x}" for b in tflite_model])
    header = f"""// Auto-generated TinyML model for Kitchen Sound Classifier
// Deploy this on ESP32 / Arduino Nano 33 BLE / Raspberry Pi Pico
// Model size: {len(tflite_model)} bytes

#ifndef {output_name.upper()}_H
#define {output_name.upper()}_H

const unsigned char {output_name}_tflite[] = {{
  {c_array}
}};
const unsigned int {output_name}_tflite_len = {len(tflite_model)};

// Class labels
const char* SOUND_CLASSES[] = {{
  "normal", "boiling_over", "gas_hissing", "smoke_alarm", "glass_breaking"
}};
const int NUM_CLASSES = {len(CLASSES)};

#endif // {output_name.upper()}_H
"""
    return header

# Generate C header
c_header = convert_to_c_array(tflite_model_quant)
with open('kitchen_model.h', 'w') as f:
    f.write(c_header)

print("Generated: kitchen_model.h")
print("This file can be included in Arduino/ESP32 sketch")
print("to run the classifier on a real microcontroller!")

print("\n" + "=" * 50)
print("SUMMARY")
print("=" * 50)
print(f"  Classes:         {len(CLASSES)} kitchen sounds")
print(f"  Test accuracy:   {test_accuracy * 100:.1f}%")
print(f"  Model size:      {tflite_quant_size:.1f} KB (quantized)")
print(f"  Input:           {N_FRAMES} frames x {N_MFCC} MFCCs")
print(f"  Output:          {len(CLASSES)} class probabilities")
print(f"  Deployment:      TFLite model + C header generated")
print(f"")
print(f"  In a real deployment:")
print(f"  - Attach MEMS microphone to ESP32/Pico")
print(f"  - Compute MFCCs from live audio")
print(f"  - Run this model for real-time classification")
print(f"  - Send results to MQTT gateway (Program 2)")
print("=" * 50)
