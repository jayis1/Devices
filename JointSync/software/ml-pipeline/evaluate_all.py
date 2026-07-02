"""
JointSync ML Pipeline — Evaluate All Models
"""

import os
import json
import numpy as np

def evaluate_all():
    """Run evaluation on all trained models and generate summary report."""
    print("=== JointSync ML Pipeline — Full Evaluation ===\n")

    results = {}

    # 1. Flare LSTM
    try:
        import torch
        from train_flare_lstm import FlarePredictionLSTM, generate_synthetic_data

        print("1. Flare Prediction LSTM")
        X_train, y_train, X_val, y_val = generate_synthetic_data(100, 50)
        model = FlarePredictionLSTM()
        if os.path.exists("models/flare_lstm_best.pt"):
            model.load_state_dict(torch.load("models/flare_lstm_best.pt"))
            model.eval()
            with torch.no_grad():
                preds = model(torch.FloatTensor(X_val)).numpy()
            from sklearn.metrics import roc_auc_score
            auc = roc_auc_score(y_val, preds)
            results["flare_lstm"] = {"auc": float(auc)}
            print(f"   AUC: {auc:.4f}")
        else:
            print("   Model not trained yet. Run train_flare_lstm.py first.")
            results["flare_lstm"] = {"status": "not_trained"}
    except Exception as e:
        print(f"   Error: {e}")
        results["flare_lstm"] = {"error": str(e)}

    # 2. Inflammation detection
    print("\n2. Inflammation Detection (Edge)")
    if os.path.exists("models/inflammation_quant.tflite"):
        import tensorflow as tf
        interpreter = tf.lite.Interpreter(model_path="models/inflammation_quant.tflite")
        interpreter.allocate_tensors()
        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()
        model_size = os.path.getsize("models/inflammation_quant.tflite")
        results["inflammation_edge"] = {
            "model_size_bytes": model_size,
            "input_shape": str(input_details[0]['shape']),
            "output_shape": str(output_details[0]['shape']),
        }
        print(f"   Model size: {model_size} bytes ({model_size/1024:.1f} KB)")
        print(f"   Input: {input_details[0]['shape']}")
        print(f"   Output: {output_details[0]['shape']}")
    else:
        print("   Model not trained yet.")
        results["inflammation_edge"] = {"status": "not_trained"}

    # 3. Activity CNN
    print("\n3. Activity CNN (Edge)")
    if os.path.exists("models/activity_cnn_quant.tflite"):
        model_size = os.path.getsize("models/activity_cnn_quant.tflite")
        results["activity_cnn"] = {"model_size_bytes": model_size}
        print(f"   Model size: {model_size} bytes ({model_size/1024:.1f} KB)")
    else:
        print("   Model not trained yet.")
        results["activity_cnn"] = {"status": "not_trained"}

    # 4. Thermal classifier
    print("\n4. Thermal Swelling Classifier")
    if os.path.exists("models/thermal_classifier.tflite"):
        model_size = os.path.getsize("models/thermal_classifier.tflite")
        results["thermal_classifier"] = {"model_size_bytes": model_size}
        print(f"   Model size: {model_size} bytes ({model_size/1024:.1f} KB)")
    else:
        print("   Model not trained yet.")
        results["thermal_classifier"] = {"status": "not_trained"}

    # 5. Gait XGBoost
    print("\n5. Gait Loading XGBoost")
    if os.path.exists("models/gait_xgboost.pkl"):
        import joblib
        model = joblib.load("models/gait_xgboost.pkl")
        results["gait_xgboost"] = {
            "n_estimators": model.n_estimators,
            "max_depth": model.max_depth,
        }
        print(f"   Estimators: {model.n_estimators}")
        print(f"   Max depth: {model.max_depth}")
    else:
        print("   Model not trained yet.")
        results["gait_xgboost"] = {"status": "not_trained"}

    # Summary
    print("\n=== Evaluation Summary ===")
    print(json.dumps(results, indent=2))

    # Save report
    with open("models/evaluation_report.json", "w") as f:
        json.dump(results, f, indent=2)
    print("\nReport saved: models/evaluation_report.json")

    return results

if __name__ == "__main__":
    evaluate_all()