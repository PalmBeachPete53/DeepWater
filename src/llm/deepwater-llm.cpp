// deepwater-llm: runs one local GGUF model for Deepwater (AI › Local), with llama.cpp (third_party/llama.cpp).
//
// The app starts it in a sandbox, Seatbelt on macOS and an AppContainer on Windows (src/LocalEngine.cpp): it may read its own model file and nothing else of
// the user's, has no network, writes nowhere and gets an empty environment. It knows nothing of Deepwater; it only
// turns conversations into answers. One process, one model; it exits when its standard input closes.
//
//   deepwater-llm --model <file.gguf> [--ctx <tokens>] [--threads <n>] [--gpu-layers <n>]
//
// Protocol: one JSON object per line on each side.
//   in   {"type":"chat","messages":[{"role":"system|user|assistant","content":"…"}],"max_tokens":n,
//         "temperature":t,"top_k":k,"top_p":p,"min_p":m,"seed":s}
//        {"type":"stop"}                               ends the answer being written
//   out  {"type":"ready","model":"…","params":n,"bytes":n,"ctx":n,"ctx_train":n,"devices":["…"],"load_ms":n}
//        {"type":"token","text":"…"}                   a piece of the answer (whole UTF-8 characters)
//        {"type":"done","stop":"eos|length|stopped|context","prompt_tokens":n,"reused_tokens":n,"tokens":n,
//         "prompt_ms":n,"gen_ms":n}
//        {"type":"error","code":"load|context|template|decode|request","message":"…"}
// The tokens of the last conversation stay in the cache: a conversation that goes on only evaluates its new part.

#include "ggml-backend.h"
#include "llama.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#endif

using json = nlohmann::json;

namespace {

std::mutex g_out;

void emit(const json &j)
{
    const std::string line = j.dump(-1, ' ', false, json::error_handler_t::replace);
    std::lock_guard<std::mutex> lock(g_out);
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void fail(const char *code, const std::string &message)
{
    emit({{"type", "error"}, {"code", code}, {"message", message}});
}

// llama.cpp's log: warnings and errors to stderr (the app shows the end of it when the process fails), the rest dropped.
void logCallback(ggml_log_level level, const char *text, void *)
{
    if (level == GGML_LOG_LEVEL_ERROR || level == GGML_LOG_LEVEL_WARN) std::fputs(text, stderr);
}

// The length of the longest prefix of `s` that ends on a whole UTF-8 character: a token can end in the middle of one.
size_t completeUtf8(const std::string &s)
{
    const size_t n = s.size();
    for (size_t back = 1; back <= 4 && back <= n; ++back) {
        const unsigned char c = static_cast<unsigned char>(s[n - back]);
        if ((c & 0xC0) == 0x80) continue;   // continuation byte: look further back
        const size_t need = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        return back >= need ? n : n - back;
    }
    return n;
}

int64_t msSince(std::chrono::steady_clock::time_point t)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t).count();
}

// The requests read from stdin by their own thread, so that a "stop" reaches the answer being written.
struct Inbox {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<json> queue;
    bool closed = false;
    std::atomic<bool> stop {false};

    void read()
    {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            json j = json::parse(line, nullptr, false);
            if (j.is_discarded() || !j.is_object()) {
                fail("request", "not a JSON object");
                continue;
            }
            if (j.value("type", "") == "stop") {
                stop = true;
                continue;
            }
            std::lock_guard<std::mutex> lock(mutex);
            queue.push_back(std::move(j));
            ready.notify_one();
        }
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
        stop = true;
        ready.notify_one();
    }

    bool next(json *out)
    {
        std::unique_lock<std::mutex> lock(mutex);
        ready.wait(lock, [this] { return closed || !queue.empty(); });
        if (queue.empty()) return false;
        *out = std::move(queue.front());
        queue.pop_front();
        stop = false;
        return true;
    }
};

struct Engine {
    llama_model *model = nullptr;
    llama_context *ctx = nullptr;
    const llama_vocab *vocab = nullptr;
    std::string chatTemplate;
    std::vector<llama_token> cached;   // the tokens in the context's memory, in order

    std::string format(const json &messages, std::string *error)
    {
        std::vector<std::string> roles, contents;
        for (const json &m : messages) {
            if (!m.is_object()) continue;
            roles.push_back(m.value("role", "user"));
            contents.push_back(m.value("content", ""));
        }
        std::vector<llama_chat_message> chat;
        size_t chars = 0;
        for (size_t i = 0; i < roles.size(); ++i) {
            chat.push_back({roles[i].c_str(), contents[i].c_str()});
            chars += contents[i].size();
        }
        std::vector<char> buf(2 * chars + 1024);
        int32_t n = llama_chat_apply_template(chatTemplate.c_str(), chat.data(), chat.size(), true, buf.data(),
                                              int32_t(buf.size()));
        if (n > int32_t(buf.size())) {
            buf.resize(size_t(n) + 1);
            n = llama_chat_apply_template(chatTemplate.c_str(), chat.data(), chat.size(), true, buf.data(),
                                          int32_t(buf.size()));
        }
        if (n < 0) {
            *error = "the model's chat template isn't one llama.cpp knows";
            return {};
        }
        return std::string(buf.data(), size_t(n));
    }

    std::vector<llama_token> tokenize(const std::string &text)
    {
        // The template already writes the special tokens; BOS is added when the model wants one.
        int32_t n = -llama_tokenize(vocab, text.c_str(), int32_t(text.size()), nullptr, 0, true, true);
        std::vector<llama_token> tokens(size_t(std::max(n, 0)));
        if (n > 0) llama_tokenize(vocab, text.c_str(), int32_t(text.size()), tokens.data(), n, true, true);
        return tokens;
    }

    llama_sampler *sampler(const json &req)
    {
        llama_sampler *chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
        const float temperature = req.value("temperature", 0.6f);
        if (temperature <= 0.0f) {
            llama_sampler_chain_add(chain, llama_sampler_init_greedy());
            return chain;
        }
        llama_sampler_chain_add(chain, llama_sampler_init_top_k(req.value("top_k", 20)));
        llama_sampler_chain_add(chain, llama_sampler_init_top_p(req.value("top_p", 0.95f), 1));
        llama_sampler_chain_add(chain, llama_sampler_init_min_p(req.value("min_p", 0.0f), 1));
        llama_sampler_chain_add(chain, llama_sampler_init_temp(temperature));
        llama_sampler_chain_add(chain, llama_sampler_init_dist(req.value("seed", uint32_t(LLAMA_DEFAULT_SEED))));
        return chain;
    }

    bool decode(llama_token *tokens, int32_t n)
    {
        const int32_t batch = int32_t(llama_n_batch(ctx));
        for (int32_t i = 0; i < n; i += batch) {
            const int32_t count = std::min(batch, n - i);
            if (llama_decode(ctx, llama_batch_get_one(tokens + i, count)) != 0) return false;
            cached.insert(cached.end(), tokens + i, tokens + i + count);
        }
        return true;
    }

    void chat(const json &req, Inbox &inbox)
    {
        std::string error;
        const std::string prompt = format(req.value("messages", json::array()), &error);
        if (!error.empty()) {
            fail("template", error);
            return;
        }
        std::vector<llama_token> tokens = tokenize(prompt);
        const int32_t nCtx = int32_t(llama_n_ctx(ctx));
        if (tokens.empty() || int32_t(tokens.size()) >= nCtx - 4) {
            fail("context", "the conversation is longer than the model's context (" + std::to_string(tokens.size()) +
                                " tokens for " + std::to_string(nCtx) + ")");
            return;
        }

        // What the memory already holds of this prompt is kept; at least the last token is evaluated again, for its
        // logits.
        size_t keep = 0;
        while (keep < cached.size() && keep < tokens.size() && cached[keep] == tokens[keep]) ++keep;
        if (keep == tokens.size()) --keep;
        llama_memory_t mem = llama_get_memory(ctx);
        if (!llama_memory_seq_rm(mem, 0, llama_pos(keep), -1)) {
            llama_memory_clear(mem, true);
            keep = 0;
        }
        cached.resize(keep);

        const auto start = std::chrono::steady_clock::now();
        if (!decode(tokens.data() + keep, int32_t(tokens.size() - keep))) {
            llama_memory_clear(mem, true);
            cached.clear();
            fail("decode", "evaluating the prompt failed");
            return;
        }
        const int64_t promptMs = msSince(start);

        llama_sampler *smpl = sampler(req);
        const int maxTokens = std::max(1, req.value("max_tokens", 4096));
        const auto genStart = std::chrono::steady_clock::now();
        std::string pending;
        std::string stop = "length";
        int generated = 0;
        char piece[256];
        while (generated < maxTokens) {
            if (inbox.stop) {
                stop = "stopped";
                break;
            }
            if (int32_t(cached.size()) >= nCtx - 1) {
                stop = "context";
                break;
            }
            llama_token token = llama_sampler_sample(smpl, ctx, -1);
            if (llama_vocab_is_eog(vocab, token)) {
                stop = "eos";
                break;
            }
            ++generated;
            const int32_t n = llama_token_to_piece(vocab, token, piece, sizeof piece, 0, false);
            if (n > 0) pending.append(piece, size_t(n));
            const size_t whole = completeUtf8(pending);
            if (whole > 0) {
                emit({{"type", "token"}, {"text", pending.substr(0, whole)}});
                pending.erase(0, whole);
            }
            if (!decode(&token, 1)) {
                stop = "context";
                break;
            }
        }
        if (!pending.empty()) emit({{"type", "token"}, {"text", pending}});
        llama_sampler_free(smpl);
        emit({{"type", "done"}, {"stop", stop}, {"prompt_tokens", tokens.size()}, {"reused_tokens", keep},
              {"tokens", generated}, {"prompt_ms", promptMs}, {"gen_ms", msSince(genStart)}});
    }
};

} // namespace

#ifdef _WIN32
// The arguments in UTF-8, which llama.cpp expects: argv holds the system's code page, which can't spell every user name.
static std::vector<std::string> arguments()
{
    std::vector<std::string> out;
    int count = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 0; i < count; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(size > 0 ? size - 1 : 0, '\0');
        if (size > 1) WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, s.data(), size, nullptr, nullptr);
        out.push_back(std::move(s));
    }
    LocalFree(wide);
    return out;
}

// deepwater --llm-sandbox-check: from inside the sandbox, what must be refused. 0 when it all is.
static int sandboxSelfTest(const std::string &file)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, nullptr, 0);
    std::wstring path(size > 0 ? size - 1 : 0, L'\0');
    if (size > 1) MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, path.data(), size);

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    const bool read = h != INVALID_HANDLE_VALUE;
    if (read) CloseHandle(h);

    const std::wstring beside = path + L".sandbox-test";
    h = CreateFileW(beside.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    const bool write = h != INVALID_HANDLE_VALUE;
    if (write) CloseHandle(h);

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    bool network = false;
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s != INVALID_SOCKET) {
        sockaddr_in to {};
        to.sin_family = AF_INET;
        to.sin_port = htons(443);
        inet_pton(AF_INET, "1.1.1.1", &to.sin_addr);
        network = connect(s, reinterpret_cast<sockaddr *>(&to), sizeof to) == 0;
        closesocket(s);
    }
    WSACleanup();
    std::printf("{\"read\":%s,\"write\":%s,\"network\":%s}\n", read ? "true" : "false", write ? "true" : "false",
                network ? "true" : "false");
    std::fflush(stdout);
    return read || write || network ? 3 : 0;
}
#endif

int main(int argc, char **argv)
{
    std::string path;
    int nCtx = 8192, threads = 0, gpuLayers = -1;
#ifdef _WIN32
    // Lines as they are written: no \r added, and no byte taken for the end of the input.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    const std::vector<std::string> args = arguments();
#else
    const std::vector<std::string> args(argv, argv + argc);
#endif
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        const std::string &a = args[i];
        if (a == "--model") path = args[i + 1];
        else if (a == "--ctx") nCtx = std::atoi(args[i + 1].c_str());
        else if (a == "--threads") threads = std::atoi(args[i + 1].c_str());
        else if (a == "--gpu-layers") gpuLayers = std::atoi(args[i + 1].c_str());
#ifdef _WIN32
        else if (a == "--sandbox-selftest") return sandboxSelfTest(args[i + 1]);
#endif
    }
    if (path.empty()) {
        std::fputs("usage: deepwater-llm --model <file.gguf> [--ctx n] [--threads n] [--gpu-layers n]\n", stderr);
        return 2;
    }

    llama_log_set(logCallback, nullptr);
    llama_backend_init();

    Engine e;
    const auto start = std::chrono::steady_clock::now();
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = gpuLayers;
    e.model = llama_model_load_from_file(path.c_str(), mp);
    if (!e.model) {
        fail("load", "llama.cpp couldn't load the model");
        return 1;
    }
    e.vocab = llama_model_get_vocab(e.model);
    const int ctxTrain = llama_model_n_ctx_train(e.model);
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = uint32_t(ctxTrain > 0 ? std::min(nCtx, ctxTrain) : nCtx);
    cp.n_batch = 512;
    if (threads > 0) {
        cp.n_threads = threads;
        cp.n_threads_batch = threads;
    }
    e.ctx = llama_init_from_model(e.model, cp);
    if (!e.ctx) {
        fail("load", "llama.cpp couldn't create a context for the model");
        llama_model_free(e.model);
        return 1;
    }
    const char *tmpl = llama_model_chat_template(e.model, nullptr);
    e.chatTemplate = tmpl ? tmpl : "chatml";

    char desc[256] = {};
    llama_model_desc(e.model, desc, sizeof desc);
    json devices = json::array();
    for (size_t i = 0; i < ggml_backend_dev_count(); ++i) devices.push_back(ggml_backend_dev_name(ggml_backend_dev_get(i)));
    emit({{"type", "ready"}, {"model", desc}, {"params", llama_model_n_params(e.model)},
          {"bytes", llama_model_size(e.model)}, {"ctx", llama_n_ctx(e.ctx)}, {"ctx_train", ctxTrain},
          {"devices", devices}, {"template", tmpl != nullptr}, {"load_ms", msSince(start)}});

    Inbox inbox;
    std::thread reader([&inbox] { inbox.read(); });
    json req;
    while (inbox.next(&req)) {
        if (req.value("type", "") == "chat") e.chat(req, inbox);
        else fail("request", "unknown request type");
    }
    reader.join();

    llama_free(e.ctx);
    llama_model_free(e.model);
    llama_backend_free();
    return 0;
}
