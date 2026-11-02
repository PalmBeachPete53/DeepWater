# Prints the versions of the machine-learning stack as one JSON line (run by Deepwater's AI tab).
import json, platform, sys
out = {"python": platform.python_version(), "executable": sys.executable}
for name, module in (("numpy", "numpy"), ("pandas", "pandas"), ("pyarrow", "pyarrow"), ("scikit-learn", "sklearn"),
                     ("lightgbm", "lightgbm"), ("xgboost", "xgboost"), ("torch", "torch"), ("joblib", "joblib")):
    try:
        m = __import__(module)
        out[name] = getattr(m, "__version__", "?")
    except Exception as e:  # missing or broken: say which
        out[name] = None
        out.setdefault("errors", {})[name] = str(e).splitlines()[0][:200]
try:
    import torch
    out["torch_mps"] = bool(torch.backends.mps.is_available())
    out["torch_cuda"] = bool(torch.cuda.is_available())
except Exception:
    pass
print("RESULT " + json.dumps(out))
