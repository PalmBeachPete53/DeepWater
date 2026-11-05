# AI

The AI page has three sections: **Models**, **Local** and **Machine learning**. One rule runs through it: a model reaches Deepwater's data only through the **skills** of the slot it sits in, each skill a set of tools the app runs on its behalf and writes to a journal. Nothing of the page you look at is sent to a model outside those tools.

## Models › Local models

The models on this computer (`~/Library/Application Support/Deepwater/Deepwater/models/`; open the folder, delete) and the **Hugging Face Hub** to get more: search (30 results with task, downloads, likes and date; gated repositories marked), the files of the chosen repository with their sizes, and downloads of the ticked files (two at a time; progress, speed, Cancel; an interrupted download resumes; the hash is checked after). Weights that are data only (GGUF, safetensors, ONNX) are ticked; files that can run code when loaded (pickle-based `.bin`, `.pt`, `.ckpt`, Keras, Python, native libraries) are marked in red, never ticked, and ticking one asks first. A Hugging Face token, entered in [Sources](help:sources) › AI, opens the gated models whose terms you accepted.

**Slots.** Four squares under the models give a model its skills. Drag a model from the list into a slot (only models with a GGUF file, the local engine's format), or from one slot to another, or an integration from Models › Integration (Claude, opencode), and tick the skills the slot grants:

- **Read Deepwater's data**: read only, through the app: the country series, golden records, IMF WEO, policy rates, exchange rates, COT positions, prices, and any page's tables and charts. No file, network or writing.
- **Trade**: the Charts page's strategy tester and live paper account.

The skills belong to the slot: a model dropped in gets that slot's skills, ✕ takes it out, and a new slot grants nothing until a box is ticked. A slot is bound to the model's files (their hashes): if a file changes, appears or goes, the slot's skills are **suspended** (red dashed well) until the model is dragged in again. Only a model in a bound slot can run.

## Models › Integration

- **Claude**: the settings of Claude through the Anthropic API — the model and the effort — with the key's state (the key is in Sources › AI, with a Test). Claude can be dragged into a slot, but nothing in the app calls it yet: its card says so and it reads nothing.
- **opencode**: the terminal AI agent reads Deepwater's tools over MCP. With **Serve Deepwater's tools to opencode** ticked, the app listens on `127.0.0.1:47821` with a token (**New token** replaces it); **Add to opencode** registers Deepwater in opencode's configuration, **Copy the configuration** gives the JSON for a hand edit. The tools are those of the slot opencode is dragged into (none outside a slot), checked at every call and journaled; at most five paper orders per session. What the tools return goes to the model opencode uses, usually at a provider.

## Local

A conversation with a model of this computer. The models offered are those of the bound slots; choose the context size (4,096 to 32,768 tokens, capped at the model's; 16,384 is the size for the Trade skill, whose tools take about 10,000 tokens in a turn) and an optional system prompt, then **Load** (refused above three quarters of the Mac's memory). Answers stream as they are written; a thinking part (Qwen3, DeepSeek-R1) is shown dim above the answer. The status line gives the tokens, the speed and the prompt's tokens already evaluated.

The engine is llama.cpp with Metal, run as a separate helper in a **sandbox**: it can read the app's code and the model's files, use the GPU, and nothing else — no network, no writing, no other file or program. The app's preferences, where the keys are, are out of its reach. The model is unloaded when its slot lets it go or its files change.

**Tools.** The skills of the model's slot are the tools it is offered, listed in its system prompt with today's date and the rule to never invent a figure. The model writes a call, the app runs it and sends the result back, at most six rounds per question; every call is checked again against the slot's skills at that moment, and a tool of a skill not granted is refused. The transcript shows each call (⚙ name, arguments, size of the data or the error).

*Read Deepwater's data*: `list_data` (what is loaded), `golden_series` (a country's golden record of an indicator, with the source of every point), `country_series`, `imf_weo`, `policy_rate`, `fx_rate`, `cot_positions`, `prices` (a symbol's summary and latest bars), `read_page` (any page and section, as its tables and charts show, without moving you; Sources and AI can't be read) and `open_page` (takes you to a page, only when you ask).

*Trade*: `backtest_help` (the strategy format, the operators, every indicator, the examples and your saved strategies), `run_backtest` (loads a strategy into the tester and runs it on the active chart, switching symbol or timeframe when asked), `backtest_trades_csv` (exports the trades and summarizes them), `paper_account` (cash, equity, positions, working orders, last fills), `paper_order`, `paper_close` and `paper_cancel` (the ticket's own code, so fills, brackets and fees behave the same). Limits: the live paper account only (never the replay's practice account), no order above the account's equity, at most five orders, closes or cancels per question, and the system prompt tells the model to trade only when asked. Paper trading is simulated; real execution has no tool.

**Journal.** Every call, run, refused or failed, is one line in `ai/tool-journal.jsonl` (time, model, slot, tool, arguments, outcome, size, duration); the last 200 are shown under the conversation, with **Show the file**.

## Side panel

The **AI** flap on the right edge of every page slides a panel to ask a model of the slots from where you are (Esc closes it, `⌘Enter` sends). Each question goes with a **context line**, shown under it: the page and what it shows (the active chart's symbol and timeframe, the country and tab open, the symbol and section, the selected country). That line holds no data: the model reads the page's tables and charts only through `read_page`, with the skill. opencode, when it sits in a slot, is one of the choices, ready without loading; the model it uses is chosen in a second list.

## Machine learning

Training classical models on Deepwater's data:

- **Python environment**: a private virtual environment the app creates from a system Python (3.10 or newer) with numpy, pandas, scikit-learn, LightGBM, XGBoost and PyTorch; the install streams to the log pane.
- **Data**: **Export Deepwater's data** writes CSV files: every country series by source, the IMF WEO, policy rates, daily FX, COT, the golden records, and ten years of daily candles for the symbols you list (SPY, QQQ, GLD, USO, TLT, VIX, EUR/USD, BTC by default).
- **Training**: the framework (scikit-learn's random forest, gradient boosting, ridge or logistic; LightGBM; XGBoost; a PyTorch perceptron), the task, the data set and target column, a derived target for time series (next period's return, direction, value or change), lags, columns to leave out, the test share and hyper-parameters as JSON. Each run gets a folder with the model and its settings, rows, split, metrics (RMSE, MAE, R² and direction hit rate; accuracy, F1 and AUC) and feature importances; the runs table lists them, a double click opens the folder.
