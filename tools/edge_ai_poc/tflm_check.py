"""Run the int8 model through TensorFlow Lite Micro kernels on the host and compare with the
TFLite reference interpreter on the test split (what the STM32H743 path executes).

  pip install tflite-micro
  python3 tflm_check.py --windows ../mower_j1939_synth/out/windows.npz --artifacts phase1_artifacts
"""
import argparse, json, os, sys
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "2"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, tensorflow as tf
import train_phase1 as T
from tflite_micro.python.tflite_micro import runtime

ap = argparse.ArgumentParser()
ap.add_argument("--windows", required=True)
ap.add_argument("--artifacts", required=True)
a = ap.parse_args()

sc = json.load(open(os.path.join(a.artifacts, "scaler.json")))
mu, sd = np.array(sc["mean"], np.float32), np.array(sc["std"], np.float32)
z = np.load(a.windows)
x = z["x"].astype(np.float32)
y = np.where(z["y"].astype(int) == 4, 0, z["y"].astype(int))  # dull blades merged into normal
_, _, te = T.group_split(z["meta"], T.SEED)
q = np.clip(np.round(((x[te] - mu) / sd) / sc["input_scale"] + sc["input_zero_point"]), -128, 127).astype(np.int8)

mb = open(os.path.join(a.artifacts, "cnn", "model_int8.tflite"), "rb").read()
lite = tf.lite.Interpreter(model_content=mb,
                           experimental_op_resolver_type=tf.lite.experimental.OpResolverType.BUILTIN_REF)
lite.allocate_tensors()
li, lo = lite.get_input_details()[0]["index"], lite.get_output_details()[0]["index"]
micro = runtime.Interpreter.from_bytes(mb)

same, maxdiff, pm = 0, 0, []
for i in range(len(q)):
    lite.set_tensor(li, q[i:i + 1]); lite.invoke(); r = lite.get_tensor(lo)[0].astype(int)
    micro.set_input(q[i:i + 1], 0); micro.invoke(); m = micro.get_output(0)[0].astype(int)
    same += int(np.array_equal(r, m)); maxdiff = max(maxdiff, int(np.abs(r - m).max())); pm.append(int(np.argmax(m)))
f, _ = T.macro_f1(y[te], np.array(pm), 4)
res = {"windows": len(q), "bit_exact_outputs": same, "max_abs_diff_int8": maxdiff,
       "tflm_macro_f1": round(f, 4), "tflm_arena_used_bytes": None}
print(json.dumps(res))
json.dump(res, open(os.path.join(a.artifacts, "tflm_check.json"), "w"), indent=1)
