# Rebuild the ONNX models from MediaPipe's Face and Hand Landmarker bundles.
#   python 3.12:  pip install tensorflow-cpu tf2onnx onnxruntime
#   python convert.py
# Downloads face_landmarker.task and hand_landmarker.task (Apache 2.0, Google), unpacks
# each detector and landmark model, converts them and checks them against the TFLite originals.
import io, urllib.request, zipfile
import numpy as np
import onnxruntime as ort
import tensorflow as tf
import tf2onnx

BASE = "https://storage.googleapis.com/mediapipe-models/"
BUNDLES = {
    BASE + "face_landmarker/face_landmarker/float16/1/face_landmarker.task": {"face_detector": 128, "face_landmarks_detector": 256},
    BASE + "hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task": {"hand_detector": 192, "hand_landmarks_detector": 224},
}

for url, models in BUNDLES.items():
  task = zipfile.ZipFile(io.BytesIO(urllib.request.urlopen(url).read()))
  for name, size in models.items():
    tflite = task.read(name + ".tflite")
    open(name + ".tflite", "wb").write(tflite)
    tf2onnx.convert.from_tflite(name + ".tflite", opset=17, output_path=name + ".onnx")

    x = np.random.rand(1, size, size, 3).astype(np.float32)
    lite = tf.lite.Interpreter(model_content=tflite)
    lite.allocate_tensors()
    lite.set_tensor(lite.get_input_details()[0]["index"], x)
    lite.invoke()
    want = [lite.get_tensor(o["index"]) for o in lite.get_output_details()]
    sess = ort.InferenceSession(name + ".onnx")
    got = sess.run(None, {sess.get_inputs()[0].name: x})
    for w in want:
        diff = min(np.abs(w.ravel() - g.ravel()).max() for g in got if g.size == w.size)
        print(name, w.shape, "max diff", diff)
