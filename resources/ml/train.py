# Trains one model on a CSV exported by Deepwater, writes the metrics, the feature importances and the model to
# the run folder, and reports progress on stdout ("PROGRESS i/n", "LOG …", "RESULT {json}"). Run by the AI tab's
# Machine learning section in the app's private Python environment.
import argparse, json, os, sys, time, platform, warnings
import numpy as np
import pandas as pd

# The libraries' deprecation notices would only clutter the app's log.
warnings.filterwarnings("ignore", category=DeprecationWarning)
warnings.filterwarnings("ignore", category=FutureWarning)

def log(msg):
    print("LOG " + str(msg), flush=True)

def progress(i, n):
    print(f"PROGRESS {i}/{n}", flush=True)

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--framework", required=True, choices=["sklearn", "lightgbm", "xgboost", "torch"])
    p.add_argument("--task", required=True, choices=["regression", "classification"])
    p.add_argument("--model", default="")            # sklearn: rf | gb | linear | logistic ; torch: mlp
    p.add_argument("--data", required=True)
    p.add_argument("--target", required=True)
    p.add_argument("--features", default="")         # comma-separated; empty = every numeric column but the target
    p.add_argument("--drop", default="")             # columns never used
    p.add_argument("--make-target", default="none", choices=["none", "next_return", "next_up", "next_value", "next_change"])
    p.add_argument("--lags", type=int, default=0)    # lagged copies of the features (time series)
    p.add_argument("--time-column", default="")      # sorts and splits by time when given (or found)
    p.add_argument("--test-size", type=float, default=0.2)
    p.add_argument("--params", default="{}")         # JSON of hyper-parameters
    p.add_argument("--out", required=True)
    return p.parse_args()

def find_time_column(df, wanted):
    if wanted and wanted in df.columns:
        return wanted
    for c in ("time", "date", "datetime", "day", "month", "year"):
        if c in df.columns:
            return c
    return ""

def build(df, args):
    time_col = find_time_column(df, args.time_column)
    if time_col:
        df = df.sort_values(time_col).reset_index(drop=True)
    target = args.target
    # Derived targets for time series: from the chosen column's next value.
    if args.make_target != "none":
        src = df[target].astype(float)
        if args.make_target == "next_return":
            y = src.pct_change().shift(-1) * 100.0
            name = f"next_return_{target}"
        elif args.make_target == "next_up":
            y = (src.shift(-1) > src).astype(float)
            name = f"next_up_{target}"
        elif args.make_target == "next_change":
            y = src.diff().shift(-1)
            name = f"next_change_{target}"
        else:
            y = src.shift(-1)
            name = f"next_{target}"
        df[name] = y
        # The current value and a few returns are natural features.
        df[f"return_{target}"] = src.pct_change() * 100.0
        target = name
    drop = set(c for c in args.drop.split(",") if c)
    if args.features:
        feats = [c for c in args.features.split(",") if c and c in df.columns and c != target]
    else:
        feats = [c for c in df.columns if c != target and c not in drop and c != time_col and pd.api.types.is_numeric_dtype(df[c])]
    if args.lags > 0:
        lagged = {}
        for c in feats:
            for k in range(1, args.lags + 1):
                lagged[f"{c}_lag{k}"] = df[c].shift(k)
        df = pd.concat([df, pd.DataFrame(lagged)], axis=1)
        feats = feats + list(lagged.keys())
    data = df[feats + [target]].replace([np.inf, -np.inf], np.nan).dropna()
    X = data[feats].astype(float).to_numpy()
    y = data[target].to_numpy()
    if args.task == "classification":
        # Labels as 0..k-1.
        classes, y = np.unique(y, return_inverse=True)
    else:
        classes = None
        y = y.astype(float)
    n_test = max(1, int(round(len(X) * args.test_size)))
    if time_col:
        X_tr, X_te, y_tr, y_te = X[:-n_test], X[-n_test:], y[:-n_test], y[-n_test:]
        split = "last %d rows by time" % n_test
    else:
        rng = np.random.default_rng(42)
        idx = rng.permutation(len(X))
        te, tr = idx[:n_test], idx[n_test:]
        X_tr, X_te, y_tr, y_te = X[tr], X[te], y[tr], y[te]
        split = "random %d rows" % n_test
    return feats, target, classes, X_tr, X_te, y_tr, y_te, split, len(data)

def metrics_of(task, y_true, y_pred, y_score=None):
    from sklearn import metrics as M
    if task == "regression":
        out = {"rmse": float(np.sqrt(M.mean_squared_error(y_true, y_pred))), "mae": float(M.mean_absolute_error(y_true, y_pred)),
               "r2": float(M.r2_score(y_true, y_pred)) if len(y_true) > 1 else None}
        # Direction hit rate: handy for returns.
        if np.std(y_true) > 0:
            out["direction_hit"] = float(np.mean(np.sign(y_true) == np.sign(y_pred)))
        return out
    out = {"accuracy": float(M.accuracy_score(y_true, y_pred)),
           "f1": float(M.f1_score(y_true, y_pred, average="binary" if len(np.unique(y_true)) <= 2 else "macro", zero_division=0))}
    if y_score is not None and len(np.unique(y_true)) == 2:
        try:
            out["auc"] = float(M.roc_auc_score(y_true, y_score))
        except Exception:
            pass
    return out

def train_sklearn(args, params, task, X_tr, y_tr):
    from sklearn.ensemble import RandomForestRegressor, RandomForestClassifier, GradientBoostingRegressor, GradientBoostingClassifier
    from sklearn.linear_model import Ridge, LogisticRegression
    from sklearn.pipeline import make_pipeline
    from sklearn.preprocessing import StandardScaler
    model = args.model or ("rf")
    n = int(params.get("n_estimators", 300))
    depth = params.get("max_depth")
    depth = int(depth) if depth not in (None, "", 0, "0") else None
    if model == "linear":
        est = make_pipeline(StandardScaler(), Ridge(alpha=float(params.get("alpha", 1.0)))) if task == "regression" \
            else make_pipeline(StandardScaler(), LogisticRegression(max_iter=2000, C=float(params.get("C", 1.0))))
    elif model == "gb":
        est = (GradientBoostingRegressor if task == "regression" else GradientBoostingClassifier)(
            n_estimators=n, max_depth=depth or 3, learning_rate=float(params.get("learning_rate", 0.05)), random_state=42)
    else:
        est = (RandomForestRegressor if task == "regression" else RandomForestClassifier)(
            n_estimators=n, max_depth=depth, n_jobs=-1, random_state=42)
    progress(1, 3)
    est.fit(X_tr, y_tr)
    progress(2, 3)
    inner = est[-1] if hasattr(est, "steps") else est
    importances = getattr(inner, "feature_importances_", None)
    if importances is None and hasattr(inner, "coef_"):
        importances = np.abs(np.atleast_2d(inner.coef_)).mean(axis=0)
    return est, importances

def train_lightgbm(args, params, task, X_tr, y_tr, X_te, y_te):
    import lightgbm as lgb
    n = int(params.get("n_estimators", 500))
    kw = dict(n_estimators=n, learning_rate=float(params.get("learning_rate", 0.03)), num_leaves=int(params.get("num_leaves", 31)),
              max_depth=int(params.get("max_depth", -1)), subsample=float(params.get("subsample", 0.8)), subsample_freq=1,
              colsample_bytree=float(params.get("colsample_bytree", 0.8)), random_state=42, verbose=-1)
    est = (lgb.LGBMRegressor if task == "regression" else lgb.LGBMClassifier)(**kw)
    seen = {"i": 0}
    def cb(env):
        seen["i"] = env.iteration + 1
        if seen["i"] % max(1, n // 20) == 0:
            progress(seen["i"], n)
    callbacks = [cb, lgb.early_stopping(int(params.get("early_stopping", 50)), verbose=False)]
    est.fit(X_tr, y_tr, eval_set=[(X_te, y_te)], callbacks=callbacks)
    log(f"best iteration {est.best_iteration_}")
    return est, est.feature_importances_.astype(float)

def train_xgboost(args, params, task, X_tr, y_tr, X_te, y_te):
    import xgboost as xgb
    n = int(params.get("n_estimators", 500))
    kw = dict(n_estimators=n, learning_rate=float(params.get("learning_rate", 0.03)), max_depth=int(params.get("max_depth", 6)),
              subsample=float(params.get("subsample", 0.8)), colsample_bytree=float(params.get("colsample_bytree", 0.8)),
              random_state=42, early_stopping_rounds=int(params.get("early_stopping", 50)), n_jobs=0)
    if task == "classification":
        kw["eval_metric"] = "logloss"
    est = (xgb.XGBRegressor if task == "regression" else xgb.XGBClassifier)(**kw)
    est.fit(X_tr, y_tr, eval_set=[(X_te, y_te)], verbose=False)
    log(f"best iteration {getattr(est, 'best_iteration', None)}")
    return est, est.feature_importances_.astype(float)

def train_torch(args, params, task, X_tr, y_tr, X_te, y_te, n_classes):
    import torch
    from torch import nn
    device = "mps" if torch.backends.mps.is_available() else ("cuda" if torch.cuda.is_available() else "cpu")
    log(f"torch device {device}")
    torch.manual_seed(42)
    mean, std = X_tr.mean(axis=0), X_tr.std(axis=0) + 1e-9
    def T(a):
        return torch.tensor(a, dtype=torch.float32, device=device)
    Xt, Xv = T((X_tr - mean) / std), T((X_te - mean) / std)
    hidden = [int(h) for h in str(params.get("hidden", "64,32")).split(",") if h]
    out_dim = 1 if task == "regression" or n_classes <= 2 else n_classes
    layers, d = [], X_tr.shape[1]
    for h in hidden:
        layers += [nn.Linear(d, h), nn.ReLU(), nn.Dropout(float(params.get("dropout", 0.1)))]
        d = h
    layers.append(nn.Linear(d, out_dim))
    net = nn.Sequential(*layers).to(device)
    if task == "regression":
        yt, yv = T(y_tr).unsqueeze(1), T(y_te).unsqueeze(1)
        loss_fn = nn.MSELoss()
    elif out_dim == 1:
        yt, yv = T(y_tr).unsqueeze(1), T(y_te).unsqueeze(1)
        loss_fn = nn.BCEWithLogitsLoss()
    else:
        yt = torch.tensor(y_tr, dtype=torch.long, device=device)
        yv = torch.tensor(y_te, dtype=torch.long, device=device)
        loss_fn = nn.CrossEntropyLoss()
    opt = torch.optim.Adam(net.parameters(), lr=float(params.get("learning_rate", 1e-3)), weight_decay=float(params.get("weight_decay", 1e-5)))
    epochs, batch = int(params.get("epochs", 200)), int(params.get("batch_size", 256))
    patience, best, best_state, bad = int(params.get("early_stopping", 20)), float("inf"), None, 0
    n = len(Xt)
    for epoch in range(1, epochs + 1):
        net.train()
        perm = torch.randperm(n, device=device)
        for i in range(0, n, batch):
            idx = perm[i:i + batch]
            opt.zero_grad()
            loss = loss_fn(net(Xt[idx]), yt[idx])
            loss.backward()
            opt.step()
        net.eval()
        with torch.no_grad():
            val = float(loss_fn(net(Xv), yv))
        if epoch % max(1, epochs // 20) == 0 or epoch == 1:
            progress(epoch, epochs)
            log(f"epoch {epoch} validation loss {val:.5f}")
        if val < best - 1e-7:
            best, bad, best_state = val, 0, {k: v.detach().clone() for k, v in net.state_dict().items()}
        else:
            bad += 1
            if bad >= patience:
                log(f"early stop at epoch {epoch}")
                break
    if best_state:
        net.load_state_dict(best_state)
    net.eval()
    with torch.no_grad():
        out = net(Xv).cpu().numpy()
    if task == "regression":
        pred, score = out[:, 0], None
    elif out_dim == 1:
        score = 1 / (1 + np.exp(-out[:, 0]))
        pred = (score > 0.5).astype(int)
    else:
        pred, score = out.argmax(axis=1), None
    # Permutation importance on the validation set: how much the loss rises when a feature is shuffled.
    rng = np.random.default_rng(0)
    base = best
    importances = []
    Xv_np = ((X_te - mean) / std).copy()
    with torch.no_grad():
        for j in range(X_te.shape[1]):
            Xs = Xv_np.copy()
            rng.shuffle(Xs[:, j])
            importances.append(float(loss_fn(net(T(Xs)), yv)) - base)
    bundle = {"state_dict": {k: v.cpu() for k, v in net.state_dict().items()}, "mean": mean, "std": std, "hidden": hidden,
              "task": task, "out_dim": out_dim, "n_features": X_tr.shape[1]}
    return bundle, np.array(importances), pred, score

def main():
    args = parse_args()
    params = json.loads(args.params or "{}")
    started = time.time()
    os.makedirs(args.out, exist_ok=True)
    log(f"reading {args.data}")
    df = pd.read_csv(args.data)
    feats, target, classes, X_tr, X_te, y_tr, y_te, split, rows = build(df, args)
    log(f"{rows} rows, {len(feats)} features, target {target}, test split: {split}")
    if len(X_tr) < 10:
        print("RESULT " + json.dumps({"error": "not enough rows after cleaning (%d)" % len(X_tr)}), flush=True)
        return 2
    n_classes = int(len(classes)) if classes is not None else 0
    pred = score = None
    if args.framework == "sklearn":
        est, importances = train_sklearn(args, params, args.task, X_tr, y_tr)
    elif args.framework == "lightgbm":
        est, importances = train_lightgbm(args, params, args.task, X_tr, y_tr, X_te, y_te)
    elif args.framework == "xgboost":
        est, importances = train_xgboost(args, params, args.task, X_tr, y_tr, X_te, y_te)
    else:
        est, importances, pred, score = train_torch(args, params, args.task, X_tr, y_tr, X_te, y_te, n_classes)
    if pred is None:
        pred = est.predict(X_te)
        if args.task == "classification" and hasattr(est, "predict_proba"):
            proba = est.predict_proba(X_te)
            score = proba[:, 1] if proba.shape[1] == 2 else None
    m = metrics_of(args.task, y_te, pred, score)
    # Save the model.
    if args.framework == "torch":
        import torch
        torch.save(est, os.path.join(args.out, "model.pt"))
        model_file = "model.pt"
    else:
        import joblib
        joblib.dump(est, os.path.join(args.out, "model.joblib"))
        model_file = "model.joblib"
        if args.framework == "lightgbm":
            est.booster_.save_model(os.path.join(args.out, "model.txt"))
        if args.framework == "xgboost":
            est.save_model(os.path.join(args.out, "model.json"))
    imp = None
    if importances is not None and len(importances) == len(feats):
        total = float(np.sum(np.abs(importances))) or 1.0
        imp = sorted([{"feature": f, "importance": float(v) / total} for f, v in zip(feats, importances)], key=lambda d: -abs(d["importance"]))
    versions = {}
    for name, module in (("sklearn", "sklearn"), ("lightgbm", "lightgbm"), ("xgboost", "xgboost"), ("torch", "torch")):
        try:
            versions[name] = __import__(module).__version__
        except Exception:
            pass
    run = {"framework": args.framework, "task": args.task, "model": args.model, "data": os.path.basename(args.data),
           "target": target, "features": feats, "rows": rows, "train_rows": int(len(X_tr)), "test_rows": int(len(X_te)),
           "split": split, "params": params, "metrics": m, "importances": imp, "model_file": model_file,
           "classes": [str(c) for c in classes] if classes is not None else None,
           "seconds": round(time.time() - started, 2), "python": platform.python_version(), "versions": versions,
           "finished": time.strftime("%Y-%m-%dT%H:%M:%S")}
    with open(os.path.join(args.out, "run.json"), "w") as f:
        json.dump(run, f, indent=1)
    print("RESULT " + json.dumps({"metrics": m, "seconds": run["seconds"], "rows": rows, "features": len(feats), "model_file": model_file}), flush=True)
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:
        import traceback
        traceback.print_exc()
        print("RESULT " + json.dumps({"error": str(e)[:300]}), flush=True)
        sys.exit(1)
