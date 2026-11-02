// Lowering of the modern JavaScript syntax V4 lacks (see JsTransform.h).
//
// A tokenizer and a recursive-descent parser for ES2025 modules build a small syntax tree (node
// kind, source span, children). It does not validate: it only has to parse valid programs right,
// and on anything it can't parse it stops, so the original text goes to the engine, which reports
// the error. Each rewrite is a list of text edits (insert, replace) at token positions, applied in
// one go; rewrites that move text (patterns moved into a function body) run again until nothing is
// left, and the passes run one after the other on the previous pass's output, each re-parsing it.
#include "JsTransform.h"

#include <QHash>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <cstring>
#include <functional>

namespace qtm::js {

namespace {

// ---------------------------------------------------------------- tokens

enum class Tk : quint8 { Eof, Name, Private, Num, BigInt, Str, Template, Regex, Punct };

struct Tok {
    Tk t = Tk::Eof;
    int start = 0, end = 0;
    bool nl = false;     // a line terminator before it
    bool tail = false;   // Template: ends with ` (else with ${)
};

struct ParseError {
    int pos;
};

bool isLineTerm(QChar c)
{
    const ushort u = c.unicode();
    return u == '\n' || u == '\r' || u == 0x2028 || u == 0x2029;
}
bool isAsciiDigit(QChar c)
{
    return c.unicode() >= '0' && c.unicode() <= '9';
}
bool isIdStart(QChar c)
{
    const ushort u = c.unicode();
    if (u < 128) return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || u == '$' || u == '_';
    return c.isLetter() || c.isSurrogate() || c.category() == QChar::Number_Letter;
}
bool isIdPart(QChar c)
{
    const ushort u = c.unicode();
    if (u < 128) return isIdStart(c) || isAsciiDigit(c);
    return c.isLetterOrNumber() || c.isMark() || c.isSurrogate() || c.category() == QChar::Punctuation_Connector
           || u == 0x200C || u == 0x200D;
}

const char *const PUNCT[] = { ">>>=", "...", "===", "!==", "**=", "<<=", ">>=", ">>>", "&&=", "||=", "?\?=", "=>", "==",
                              "!=",   "<=",  ">=",  "&&",  "||",  "??",  "?.",  "++",  "--",  "+=",  "-=",  "*=",  "/=",
                              "%=",   "&=",  "|=",  "^=",  "<<",  ">>",  "**" };

class Lexer
{
public:
    explicit Lexer(const QString &s) : s(s), n(int(s.size())) { }
    const QString &s;
    const int n;

    QChar at(int p) const { return p < n ? s[p] : QChar(); }

    int skip(int p, bool &nl) const
    {
        for (;;) {
            if (p >= n) return p;
            const QChar c = s[p];
            const ushort u = c.unicode();
            if (u == ' ' || u == '\t' || u == 0x0B || u == 0x0C || u == 0xA0 || u == 0xFEFF) {
                p++;
            } else if (isLineTerm(c)) {
                nl = true;
                p++;
            } else if (u == '/' && at(p + 1) == QLatin1Char('/')) {
                while (p < n && !isLineTerm(s[p])) p++;
            } else if (u == '/' && at(p + 1) == QLatin1Char('*')) {
                const int e = int(s.indexOf(QLatin1String("*/"), p + 2));
                if (e < 0) throw ParseError { p };
                for (int i = p + 2; i < e && !nl; i++)
                    if (isLineTerm(s[i])) nl = true;
                p = e + 2;
            } else if (u == '#' && p == 0 && at(1) == QLatin1Char('!')) {   // hashbang
                while (p < n && !isLineTerm(s[p])) p++;
            } else if (u > 127 && c.category() == QChar::Separator_Space) {
                p++;
            } else {
                return p;
            }
        }
    }

    int ident(int p) const
    {
        while (p < n) {
            if (s[p] == QLatin1Char('\\') && at(p + 1) == QLatin1Char('u')) {
                p += 2;
                if (at(p) == QLatin1Char('{')) {
                    while (p < n && s[p] != QLatin1Char('}')) p++;
                    p++;
                } else {
                    p += 4;
                }
            } else if (isIdPart(s[p])) {
                p++;
            } else {
                break;
            }
        }
        return std::min(p, n);
    }

    Tok lex(int p) const
    {
        bool nl = false;
        p = skip(p, nl);
        Tok t;
        t.nl = nl;
        t.start = p;
        if (p >= n) {
            t.end = p;
            return t;
        }
        const QChar c = s[p];
        const ushort u = c.unicode();
        if (isIdStart(c) || u == '\\') {
            t.t = Tk::Name;
            t.end = ident(p);
            if (t.end == p) throw ParseError { p };
            return t;
        }
        if (u == '#') {
            t.t = Tk::Private;
            t.end = ident(p + 1);
            if (t.end == p + 1) throw ParseError { p };
            return t;
        }
        if (isAsciiDigit(c) || (u == '.' && isAsciiDigit(at(p + 1)))) return number(p, nl);
        if (u == '"' || u == '\'') {
            int q = p + 1;
            for (;;) {
                if (q >= n) throw ParseError { p };
                const QChar ch = s[q];
                if (ch == c) {
                    q++;
                    break;
                }
                if (ch == QLatin1Char('\\')) {
                    q += 2;
                    if (at(q - 1) == QLatin1Char('\r') && at(q) == QLatin1Char('\n')) q++;
                    continue;
                }
                if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) throw ParseError { p };
                q++;
            }
            t.t = Tk::Str;
            t.end = q;
            return t;
        }
        if (u == '`') return templ(p, nl);
        for (const char *pp : PUNCT) {
            const int len = int(strlen(pp));
            bool ok = p + len <= n;
            for (int i = 0; ok && i < len; i++) ok = s[p + i] == QLatin1Char(pp[i]);
            if (!ok) continue;
            if (len == 2 && pp[0] == '?' && pp[1] == '.' && isAsciiDigit(at(p + 2))) continue;   // a ?.5 : b
            t.t = Tk::Punct;
            t.end = p + len;
            return t;
        }
        if (u < 128 && strchr("{}()[];,<>+-*/%&|^!~?:=.@", char(u))) {
            t.t = Tk::Punct;
            t.end = p + 1;
            return t;
        }
        throw ParseError { p };
    }

    Tok number(int p, bool nl) const
    {
        Tok t;
        t.nl = nl;
        t.start = p;
        int q = p;
        auto digits = [&](bool hex) {
            while (q < n) {
                const QChar ch = s[q];
                const ushort u = ch.unicode();
                if (isAsciiDigit(ch) || u == '_' || (hex && ((u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F')))) q++;
                else break;
            }
        };
        bool simple = true;
        const QChar c1 = at(p + 1);
        if (s[p] == QLatin1Char('0') && QStringLiteral("xXoObB").contains(c1)) {
            q += 2;
            digits(true);
        } else {
            digits(false);
            if (at(q) == QLatin1Char('.')) {
                simple = false;
                q++;
                digits(false);
            }
            if (at(q) == QLatin1Char('e') || at(q) == QLatin1Char('E')) {
                simple = false;
                q++;
                if (at(q) == QLatin1Char('+') || at(q) == QLatin1Char('-')) q++;
                digits(false);
            }
        }
        t.t = Tk::Num;
        if (simple && at(q) == QLatin1Char('n')) {
            q++;
            t.t = Tk::BigInt;
        }
        if (q < n && isIdStart(s[q])) throw ParseError { q };
        t.end = q;
        return t;
    }

    // p at ` (a template's start) or at } (after a substitution).
    Tok templ(int p, bool nl) const
    {
        Tok t;
        t.t = Tk::Template;
        t.nl = nl;
        t.start = p;
        int q = p + 1;
        for (;;) {
            if (q >= n) throw ParseError { p };
            const QChar ch = s[q];
            if (ch == QLatin1Char('`')) {
                t.tail = true;
                t.end = q + 1;
                return t;
            }
            if (ch == QLatin1Char('$') && at(q + 1) == QLatin1Char('{')) {
                t.end = q + 2;
                return t;
            }
            q += ch == QLatin1Char('\\') ? 2 : 1;
        }
    }

    // p at the opening /.
    Tok regex(int p, bool nl) const
    {
        Tok t;
        t.t = Tk::Regex;
        t.nl = nl;
        t.start = p;
        int q = p + 1;
        bool cls = false;
        for (;;) {
            if (q >= n || isLineTerm(s[q])) throw ParseError { p };
            const QChar ch = s[q];
            if (ch == QLatin1Char('\\')) {
                q += 2;
                continue;
            }
            if (ch == QLatin1Char('[')) cls = true;
            else if (ch == QLatin1Char(']')) cls = false;
            else if (ch == QLatin1Char('/') && !cls) break;
            q++;
        }
        q++;
        while (q < n && isIdPart(s[q])) q++;
        t.end = q;
        return t;
    }
};

// ---------------------------------------------------------------- syntax tree

enum NT : quint8 {
    Program, VarDecl, Declarator, Func, Class, Method, Field, StaticBlock, Block, ExprStmt, If, For, ForIn, ForOf,
    While, DoWhile, Return, Throw, Jump, Try, Catch, Switch, Case, Labeled, Empty, Debugger, With, ImportDecl,
    ExportDecl, Ident, Lit, BigIntLit, RegexLit, Template, Tagged, This, Super, ArrayX, ObjectX, Prop, Spread, Rest,
    ArrayPat, ObjectPat, AssignPat, Unary, Update, Binary, Assign, Cond, Call, New, Member, Seq, Yield, Await,
    Paren, ImportCall, MetaProp, PrivName, PrivIn
};

enum : int {
    F_ASYNC = 1 << 0, F_GEN = 1 << 1, F_ARROW = 1 << 2, F_CONCISE = 1 << 3, F_DECL = 1 << 4, F_STATIC = 1 << 5,
    F_COMPUTED = 1 << 6, F_SHORTHAND = 1 << 7, F_METHOD = 1 << 8, F_GET = 1 << 9, F_SET = 1 << 10,
    F_OPTIONAL = 1 << 11, F_DELEGATE = 1 << 12, F_AWAIT = 1 << 13, F_CTOR = 1 << 14, F_DEFAULT = 1 << 15,
    F_STAR = 1 << 16, F_STRING = 1 << 17,
};

struct Node {
    NT t;
    int start = 0, end = 0;
    int flags = 0;
    int a = -1, b = -1, c = -1, d = -1;
    int x0 = -1, x1 = -1, x2 = -1;   // token offsets, meaning depends on the kind (see the parser)
    QVector<int> list;
    QVector<int> seps;               // offsets of the commas between list items
    QString str, str2;
};

// What each kind keeps in its slots:
//   Func      a name, list params, b body (Block, or an expression when F_CONCISE); x0 `async`, x1 `(`, x2 after `)`,
//             d `*` of a generator
//   Class     a name, b superclass, list elements; x0 body `{`, x1 body `}`, x2 end of `class`
//   Method    a key, b Func; x0 element start, x1 key start (`[` when computed), x2 key end
//   Field     a key, b initializer; x0 element start, x1/x2 key span, d `=`, c `;`
//   StaticBlock a Block; x0 `static`
//   Prop      a key, b value (pattern when in a pattern); x1/x2 key span
//   Spread/Rest a argument; x0 `...`
//   Member    a object, b property; x0 `.` or `?.`, x1 `[`, x2 `]`
//   Call/New  a callee, list arguments; x0 `(`, x1 `)`
//   Assign    a target, b value, str operator; x0 operator
//   ForIn/Of  a left, b right, c body; x0 `await`, x1 `(`, x2 `)`, d end of `of`/`in`
//   Catch     a parameter, b body; x0 end of `catch`
//   PrivIn    a PrivName, b right; x0 end of `in`
//   RegexLit  x0 pattern start, x1 pattern end (the closing /), x2 end
//   ImportCall x0 `(`;  ExportDecl str source text, str2 `as` name (export * as)

class Parser
{
public:
    explicit Parser(const QString &s) : src(s), lx(s) { }

    const QString &src;
    Lexer lx;
    QVector<Node> N;
    QStringList imports;
    int root = -1;

    void run()
    {
        tok = lx.lex(0);
        root = mk(Program, 0);
        QVector<int> body;
        while (tok.t != Tk::Eof) body << statement();
        N[root].list = body;
        N[root].end = int(src.size());
    }

    QStringView text(int id) const { return QStringView(src).mid(N[id].start, N[id].end - N[id].start); }

private:
    Tok tok;
    int prevEnd = 0;
    bool inGen = false;

    struct GenScope {
        GenScope(Parser &p, bool gen) : p(p), saved(p.inGen) { p.inGen = gen; }
        ~GenScope() { p.inGen = saved; }
        Parser &p;
        bool saved;
    };

    // ---- token helpers
    bool eq(const Tok &t, const char *w) const
    {
        const int len = int(strlen(w));
        if (t.end - t.start != len) return false;
        for (int i = 0; i < len; i++)
            if (src[t.start + i] != QLatin1Char(w[i])) return false;
        return true;
    }
    bool isP(const Tok &t, const char *p) const { return t.t == Tk::Punct && eq(t, p); }
    bool is(const char *p) const { return isP(tok, p); }
    bool isName(const char *w) const { return tok.t == Tk::Name && eq(tok, w); }
    QString tokText(const Tok &t) const { return src.mid(t.start, t.end - t.start); }
    void next()
    {
        prevEnd = tok.end;
        tok = lx.lex(tok.end);
    }
    Tok peek() const { return lx.lex(tok.end); }
    void expect(const char *p)
    {
        if (!is(p)) throw ParseError { tok.start };
        next();
    }
    void semi()
    {
        if (is(";")) next();
        else if (!(is("}") || tok.t == Tk::Eof || tok.nl)) throw ParseError { tok.start };
    }
    bool reserved(const Tok &t) const
    {
        static const char *const words[] = { "break",  "case",       "catch",  "class",  "const",   "continue", "debugger",
                                             "default", "delete",    "do",     "else",   "export",  "extends",  "false",
                                             "finally", "for",       "function", "if",   "import",  "in",       "instanceof",
                                             "new",    "null",       "return", "super",  "switch",  "this",     "throw",
                                             "true",   "try",        "typeof", "var",    "void",    "while",    "with",
                                             "enum",   "await",      "yield",  "let",    "static",  "implements", "interface",
                                             "package", "private",   "protected", "public" };
        if (t.t != Tk::Name) return false;
        for (const char *w : words)
            if (eq(t, w)) return true;
        return false;
    }

    int mk(NT t, int start)
    {
        Node n;
        n.t = t;
        n.start = start;
        N.push_back(n);
        return int(N.size()) - 1;
    }
    int fin(int id)
    {
        N[id].end = prevEnd;
        return id;
    }
    bool isArrow(int id) const { return N[id].t == Func && (N[id].flags & F_ARROW); }

    // ---- statements
    int statement()
    {
        const int start = tok.start;
        if (is("{")) return block();
        if (is(";")) {
            next();
            return fin(mk(Empty, start));
        }
        if (tok.t == Tk::Name) {
            if (isName("var") || isName("const")) {
                const int d = varDecl(false);
                semi();
                return fin(d);
            }
            if (isName("let")) {
                const Tok pk = peek();
                if (pk.t == Tk::Name || isP(pk, "[") || isP(pk, "{")) {
                    const int d = varDecl(false);
                    semi();
                    return fin(d);
                }
            }
            if (isName("function")) return function(start, F_DECL, -1);
            if (isName("async")) {
                const Tok pk = peek();
                if (!pk.nl && eq(pk, "function") && pk.t == Tk::Name) {
                    next();
                    return function(start, F_DECL | F_ASYNC, start);
                }
            }
            if (isName("class")) return classDef(true, start);
            if (isName("if")) {
                const int id = mk(If, start);
                next();
                expect("(");
                const int test = expression(false);
                expect(")");
                const int cons = statement();
                int alt = -1;
                if (isName("else")) {
                    next();
                    alt = statement();
                }
                N[id].a = test;
                N[id].b = cons;
                N[id].c = alt;
                return fin(id);
            }
            if (isName("for")) return forStatement();
            if (isName("while")) {
                const int id = mk(While, start);
                next();
                expect("(");
                const int test = expression(false);
                expect(")");
                const int body = statement();
                N[id].a = test;
                N[id].b = body;
                return fin(id);
            }
            if (isName("do")) {
                const int id = mk(DoWhile, start);
                next();
                const int body = statement();
                if (!isName("while")) throw ParseError { tok.start };
                next();
                expect("(");
                const int test = expression(false);
                expect(")");
                if (is(";")) next();
                N[id].a = body;
                N[id].b = test;
                return fin(id);
            }
            if (isName("return") || isName("throw")) {
                const bool ret = isName("return");
                const int id = mk(ret ? Return : Throw, start);
                next();
                if (!ret || !(is(";") || is("}") || tok.t == Tk::Eof || tok.nl)) N[id].a = expression(false);
                semi();
                return fin(id);
            }
            if (isName("break") || isName("continue")) {
                const int id = mk(Jump, start);
                next();
                if (tok.t == Tk::Name && !tok.nl && !reserved(tok)) next();
                semi();
                return fin(id);
            }
            if (isName("try")) {
                const int id = mk(Try, start);
                next();
                N[id].a = block();
                if (isName("catch")) {
                    const int c = mk(Catch, tok.start);
                    next();
                    N[c].x0 = prevEnd;
                    if (is("(")) {
                        next();
                        const int param = bindingTarget();
                        N[c].a = param;
                        expect(")");
                    }
                    const int body = block();
                    N[c].b = body;
                    N[id].b = fin(c);
                }
                if (isName("finally")) {
                    next();
                    const int f = block();
                    N[id].c = f;
                }
                return fin(id);
            }
            if (isName("switch")) {
                const int id = mk(Switch, start);
                next();
                expect("(");
                N[id].a = expression(false);
                expect(")");
                expect("{");
                QVector<int> cases;
                while (!is("}")) {
                    const int c = mk(Case, tok.start);
                    if (isName("case")) {
                        next();
                        N[c].a = expression(false);
                    } else if (isName("default")) {
                        next();
                    } else {
                        throw ParseError { tok.start };
                    }
                    expect(":");
                    QVector<int> body;
                    while (!is("}") && !isName("case") && !isName("default")) body << statement();
                    N[c].list = body;
                    cases << fin(c);
                }
                next();
                N[id].list = cases;
                return fin(id);
            }
            if (isName("with")) {
                const int id = mk(With, start);
                next();
                expect("(");
                N[id].a = expression(false);
                expect(")");
                N[id].b = statement();
                return fin(id);
            }
            if (isName("debugger")) {
                next();
                semi();
                return fin(mk(Debugger, start));
            }
            if (isName("import")) {
                const Tok pk = peek();
                if (!isP(pk, "(") && !isP(pk, ".")) return importDecl();
            }
            if (isName("export")) return exportDecl();
            if (!reserved(tok) && isP(peek(), ":")) {
                const int id = mk(Labeled, start);
                next();
                next();
                N[id].a = statement();
                return fin(id);
            }
        }
        const int id = mk(ExprStmt, start);
        N[id].a = expression(false);
        semi();
        return fin(id);
    }

    int block()
    {
        const int id = mk(Block, tok.start);
        expect("{");
        QVector<int> body;
        while (!is("}")) {
            if (tok.t == Tk::Eof) throw ParseError { tok.start };
            body << statement();
        }
        next();
        N[id].list = body;
        return fin(id);
    }

    int varDecl(bool noIn)
    {
        const int id = mk(VarDecl, tok.start);
        N[id].str = tokText(tok);
        next();
        QVector<int> decls;
        for (;;) {
            const int d = mk(Declarator, tok.start);
            N[d].a = bindingTarget();
            if (is("=")) {
                next();
                N[d].b = assign(noIn);
            }
            decls << fin(d);
            if (!is(",")) break;
            N[id].seps << tok.start;
            next();
        }
        N[id].list = decls;
        return fin(id);
    }

    int forStatement()
    {
        const int start = tok.start;
        next();
        int awaitPos = -1;
        if (isName("await")) {
            awaitPos = tok.start;
            next();
        }
        const int open = tok.start;
        expect("(");
        int init = -1;
        if (is(";")) {
        } else if (isName("var") || isName("const")
                   || (isName("let") && (peek().t == Tk::Name || isP(peek(), "[") || isP(peek(), "{")))) {
            init = varDecl(true);
        } else {
            init = expression(true);
        }
        if (init >= 0 && (isName("of") || isName("in"))) {
            const bool of = isName("of");
            const int id = mk(of ? ForOf : ForIn, start);
            next();
            N[id].d = prevEnd;
            if (N[init].t != VarDecl) init = toPattern(init);
            N[id].a = init;
            N[id].b = of ? assign(false) : expression(false);
            N[id].x2 = tok.start;
            expect(")");
            N[id].c = statement();
            N[id].x0 = awaitPos;
            N[id].x1 = open;
            if (awaitPos >= 0) N[id].flags |= F_AWAIT;
            return fin(id);
        }
        const int id = mk(For, start);
        expect(";");
        int test = -1, update = -1;
        if (!is(";")) test = expression(false);
        expect(";");
        if (!is(")")) update = expression(false);
        expect(")");
        N[id].a = init;
        N[id].b = test;
        N[id].c = update;
        N[id].d = statement();
        return fin(id);
    }

    QString stringValue(const Tok &t) const { return src.mid(t.start + 1, t.end - t.start - 2); }

    void skipAttributes()
    {
        if ((isName("with") || isName("assert")) && !tok.nl) {
            next();
            expect("{");
            while (!is("}")) {
                if (tok.t == Tk::Eof) throw ParseError { tok.start };
                next();
            }
            next();
        }
    }

    int importDecl()
    {
        const int id = mk(ImportDecl, tok.start);
        next();
        if (tok.t != Tk::Str) {
            if (tok.t == Tk::Name && !isName("from")) {
                next();
                if (is(",")) next();
            } else if (isName("from") && peek().t == Tk::Name) {
                next();   // a default import named `from`
                if (is(",")) next();
            }
            if (is("*")) {
                next();
                if (!isName("as")) throw ParseError { tok.start };
                next();
                next();
            } else if (is("{")) {
                while (!is("}")) {
                    if (tok.t == Tk::Eof) throw ParseError { tok.start };
                    next();
                }
                next();
            }
            if (!isName("from")) throw ParseError { tok.start };
            next();
        }
        if (tok.t != Tk::Str) throw ParseError { tok.start };
        N[id].str = tokText(tok);
        imports << stringValue(tok);
        next();
        skipAttributes();
        semi();
        return fin(id);
    }

    int exportDecl()
    {
        const int start = tok.start;
        const int id = mk(ExportDecl, start);
        next();
        if (isName("default")) {
            N[id].flags |= F_DEFAULT;
            next();
            const int s = tok.start;
            if (isName("function")) {
                N[id].a = function(s, F_DECL, -1);
            } else if (isName("async") && !peek().nl && eq(peek(), "function")) {
                next();
                N[id].a = function(s, F_DECL | F_ASYNC, s);
            } else if (isName("class")) {
                N[id].a = classDef(true, s);
            } else {
                N[id].a = assign(false);
                semi();
            }
            return fin(id);
        }
        if (is("*")) {
            N[id].flags |= F_STAR;
            next();
            if (isName("as")) {
                next();
                N[id].str2 = tokText(tok);
                next();
            }
            if (!isName("from")) throw ParseError { tok.start };
            next();
            if (tok.t != Tk::Str) throw ParseError { tok.start };
            N[id].str = tokText(tok);
            imports << stringValue(tok);
            next();
            skipAttributes();
            semi();
            return fin(id);
        }
        if (is("{")) {
            while (!is("}")) {
                if (tok.t == Tk::Eof) throw ParseError { tok.start };
                next();
            }
            next();
            if (isName("from")) {
                next();
                if (tok.t != Tk::Str) throw ParseError { tok.start };
                N[id].str = tokText(tok);
                imports << stringValue(tok);
                next();
                skipAttributes();
            }
            semi();
            return fin(id);
        }
        N[id].a = statement();
        return fin(id);
    }

    // ---- functions and classes
    int function(int start, int flags, int asyncPos)
    {
        next();   // function
        int star = -1;
        if (is("*")) {
            flags |= F_GEN;
            star = tok.start;
            next();
        }
        const int id = mk(Func, start);
        N[id].flags = flags;
        N[id].d = star;
        N[id].x0 = asyncPos;
        if (tok.t == Tk::Name) N[id].a = ident();
        paramsAndBody(id);
        return fin(id);
    }

    void paramsAndBody(int id)
    {
        GenScope g(*this, N[id].flags & F_GEN);
        N[id].x1 = tok.start;
        expect("(");
        QVector<int> params;
        while (!is(")")) {
            const int ps = tok.start;
            if (is("...")) {
                const int r = mk(Rest, ps);
                N[r].x0 = ps;
                next();
                N[r].a = bindingTarget();
                params << fin(r);
            } else {
                const int t = bindingTarget();
                if (is("=")) {
                    next();
                    const int ap = mk(AssignPat, ps);
                    const int def = assign(false);
                    N[ap].a = t;
                    N[ap].b = def;
                    params << fin(ap);
                } else {
                    params << t;
                }
            }
            if (!is(")")) {
                N[id].seps << tok.start;
                expect(",");
            }
        }
        next();
        N[id].x2 = prevEnd;
        N[id].list = params;
        N[id].b = block();
    }

    int arrow(int start, const QVector<int> &params, int flags, int asyncPos, int open, int close)
    {
        if (!is("=>") || tok.nl) throw ParseError { tok.start };
        next();
        const int id = mk(Func, start);
        N[id].flags = flags | F_ARROW;
        N[id].x0 = asyncPos;
        N[id].x1 = open;
        N[id].x2 = close;
        N[id].list = params;
        GenScope g(*this, false);
        if (is("{")) {
            N[id].b = block();
        } else {
            N[id].flags |= F_CONCISE;
            N[id].b = assign(false);
        }
        return fin(id);
    }

    int classDef(bool decl, int start)
    {
        next();   // class
        const int id = mk(Class, start);
        N[id].x2 = prevEnd;
        if (decl) N[id].flags |= F_DECL;
        if (tok.t == Tk::Name && !isName("extends")) N[id].a = ident();
        if (isName("extends")) {
            next();
            N[id].b = lhs(false);
        }
        N[id].x0 = tok.start;
        expect("{");
        QVector<int> elements;
        while (!is("}")) {
            if (tok.t == Tk::Eof) throw ParseError { tok.start };
            if (is(";")) {
                next();
                continue;
            }
            elements << classElement();
        }
        N[id].x1 = tok.start;
        next();
        N[id].list = elements;
        return fin(id);
    }

    bool elementEnd(const Tok &t) const
    {
        return t.t == Tk::Eof || isP(t, "(") || isP(t, "=") || isP(t, ";") || isP(t, "}") || isP(t, ",") || isP(t, ":");
    }

    int classElement()
    {
        const int es = tok.start;
        int flags = 0, asyncPos = -1;
        if (isName("static")) {
            const Tok pk = peek();
            if (isP(pk, "{")) {
                const int id = mk(StaticBlock, es);
                N[id].x0 = es;
                next();
                GenScope g(*this, false);
                N[id].a = block();
                return fin(id);
            }
            if (!elementEnd(pk)) {
                flags |= F_STATIC;
                next();
            }
        }
        if (isName("async")) {
            const Tok pk = peek();
            if (!elementEnd(pk) && !pk.nl) {
                flags |= F_ASYNC;
                asyncPos = tok.start;
                next();
            }
        }
        int star = -1;
        if (is("*")) {
            flags |= F_GEN;
            star = tok.start;
            next();
        }
        if ((isName("get") || isName("set")) && !(flags & (F_ASYNC | F_GEN))) {
            const Tok pk = peek();
            if (!elementEnd(pk)) {
                flags |= isName("get") ? F_GET : F_SET;
                next();
            }
        }
        const int ks = tok.start;
        bool computed = false;
        const int key = propertyKey(computed);
        const int ke = prevEnd;
        if (computed) flags |= F_COMPUTED;
        if (is("(")) {
            const int id = mk(Method, es);
            N[id].x0 = es;
            N[id].x1 = ks;
            N[id].x2 = ke;
            const int fn = mk(Func, tok.start);
            N[fn].flags = flags & (F_ASYNC | F_GEN);
            N[fn].x0 = asyncPos;
            N[fn].d = star;
            paramsAndBody(fn);
            fin(fn);
            if (!(flags & (F_STATIC | F_COMPUTED)) && (N[key].t == Ident || N[key].t == Lit)
                && (N[key].str == QLatin1String("constructor") || text(key) == QLatin1String("\"constructor\"")
                    || text(key) == QLatin1String("'constructor'")))
                flags |= F_CTOR;
            N[id].flags = flags;
            N[id].a = key;
            N[id].b = fn;
            return fin(id);
        }
        if (flags & (F_ASYNC | F_GEN | F_GET | F_SET)) throw ParseError { tok.start };
        const int id = mk(Field, es);
        N[id].flags = flags;
        N[id].x0 = es;
        N[id].x1 = ks;
        N[id].x2 = ke;
        N[id].a = key;
        if (is("=")) {
            N[id].d = tok.start;
            next();
            GenScope g(*this, false);
            const int init = assign(false);
            N[id].b = init;
        }
        if (is(";")) {
            N[id].c = tok.start;
            next();
        } else if (!(is("}") || tok.nl)) {
            throw ParseError { tok.start };
        }
        return fin(id);
    }

    int propertyKey(bool &computed)
    {
        computed = false;
        const int start = tok.start;
        if (is("[")) {
            next();
            const int e = assign(false);
            expect("]");
            computed = true;
            return e;
        }
        int id = -1;
        switch (tok.t) {
        case Tk::Name:
            id = mk(Ident, start);
            N[id].str = tokText(tok);
            break;
        case Tk::Str:
            id = mk(Lit, start);
            N[id].flags |= F_STRING;
            break;
        case Tk::Num: id = mk(Lit, start); break;
        case Tk::BigInt: id = mk(BigIntLit, start); break;
        case Tk::Private:
            id = mk(PrivName, start);
            N[id].str = tokText(tok);
            break;
        default: throw ParseError { start };
        }
        next();
        return fin(id);
    }

    // ---- patterns
    int bindingTarget()
    {
        if (tok.t == Tk::Name) {
            if (reserved(tok)) throw ParseError { tok.start };   // const if = 1
            return ident();
        }
        if (is("[")) return toPattern(arrayLit());
        if (is("{")) return toPattern(objectLit());
        throw ParseError { tok.start };
    }

    int toPattern(int id)
    {
        switch (N[id].t) {
        case ObjectX:
            N[id].t = ObjectPat;
            for (int p : N[id].list) {
                if (N[p].t == Spread) {
                    N[p].t = Rest;
                    N[p].a = toPattern(N[p].a);
                } else if (N[p].t == Prop) {
                    if (N[p].flags & (F_METHOD | F_GET | F_SET)) throw ParseError { N[p].start };
                    N[p].b = toPattern(N[p].b);
                }
            }
            return id;
        case ArrayX:
            N[id].t = ArrayPat;
            for (int i = 0; i < N[id].list.size(); i++) {
                const int el = N[id].list[i];
                if (el < 0) continue;
                if (N[el].t == Spread) {
                    N[el].t = Rest;
                    N[el].a = toPattern(N[el].a);
                } else {
                    toPattern(el);
                }
            }
            return id;
        case Assign:
            if (N[id].str != QLatin1String("=")) throw ParseError { N[id].start };
            N[id].t = AssignPat;
            N[id].a = toPattern(N[id].a);
            return id;
        case Ident:
        case Member:
        case Paren:
        case ObjectPat:
        case ArrayPat:
        case AssignPat:
        case Rest: return id;
        default: throw ParseError { N[id].start };
        }
    }

    // ---- expressions
    int ident()
    {
        if (tok.t != Tk::Name) throw ParseError { tok.start };
        const int id = mk(Ident, tok.start);
        N[id].str = tokText(tok);
        next();
        return fin(id);
    }

    int expression(bool noIn)
    {
        const int start = tok.start;
        const int first = assign(noIn);
        if (!is(",")) return first;
        const int id = mk(Seq, start);
        QVector<int> items { first };
        while (is(",")) {
            N[id].seps << tok.start;
            next();
            items << assign(noIn);
        }
        N[id].list = items;
        return fin(id);
    }

    bool assignOp() const
    {
        static const char *const ops[] = { "=", "+=", "-=", "*=", "/=", "%=", "**=", "<<=", ">>=", ">>>=", "&=", "|=", "^=", "&&=", "||=", "?\?=" };
        if (tok.t != Tk::Punct) return false;
        for (const char *o : ops)
            if (eq(tok, o)) return true;
        return false;
    }

    int assign(bool noIn)
    {
        const int start = tok.start;
        if (inGen && isName("yield")) {
            const int id = mk(Yield, start);
            next();
            if (is("*")) {
                N[id].flags |= F_DELEGATE;
                next();
                N[id].a = assign(noIn);
            } else if (!tok.nl && !(is(")") || is("]") || is("}") || is(",") || is(";") || is(":") || tok.t == Tk::Eof
                                    || (is("in") && noIn))) {
                N[id].a = assign(noIn);
            }
            return fin(id);
        }
        if (tok.t == Tk::Name && !reserved(tok)) {
            const Tok pk = peek();
            if (isP(pk, "=>") && !pk.nl) {
                const int p = ident();
                return arrow(start, { p }, 0, -1, -1, prevEnd);
            }
            if (eq(tok, "async") && pk.t == Tk::Name && !pk.nl && !eq(pk, "function")) {
                const Tok pk2 = lx.lex(pk.end);
                if (isP(pk2, "=>") && !pk2.nl) {
                    next();
                    const int p = ident();
                    return arrow(start, { p }, F_ASYNC, start, -1, prevEnd);
                }
            }
        }
        int left = conditional(noIn);
        if (isArrow(left)) return left;
        if (assignOp()) {
            const QString op = tokText(tok);
            const int opPos = tok.start;
            if (op == QLatin1String("=") && (N[left].t == ObjectX || N[left].t == ArrayX)) left = toPattern(left);
            next();
            const int right = assign(noIn);
            const int id = mk(Assign, start);
            N[id].a = left;
            N[id].b = right;
            N[id].str = op;
            N[id].x0 = opPos;
            return fin(id);
        }
        return left;
    }

    int conditional(bool noIn)
    {
        const int start = tok.start;
        const int test = binary(1, noIn);
        if (isArrow(test) || !is("?")) return test;
        next();
        const int cons = assign(false);
        expect(":");
        const int alt = assign(noIn);
        const int id = mk(Cond, start);
        N[id].a = test;
        N[id].b = cons;
        N[id].c = alt;
        return fin(id);
    }

    int precedence(bool noIn) const
    {
        if (tok.t == Tk::Name) {
            if (eq(tok, "instanceof")) return 7;
            if (eq(tok, "in")) return noIn ? 0 : 7;
            return 0;
        }
        if (tok.t != Tk::Punct) return 0;
        static const struct {
            const char *op;
            int prec;
        } table[] = { { "??", 1 }, { "||", 1 },  { "&&", 2 },  { "|", 3 },   { "^", 4 },   { "&", 5 },  { "==", 6 },
                      { "!=", 6 }, { "===", 6 }, { "!==", 6 }, { "<", 7 },   { ">", 7 },   { "<=", 7 }, { ">=", 7 },
                      { "<<", 8 }, { ">>", 8 },  { ">>>", 8 }, { "+", 9 },   { "-", 9 },   { "*", 10 }, { "/", 10 },
                      { "%", 10 }, { "**", 11 } };
        for (const auto &e : table)
            if (eq(tok, e.op)) return e.prec;
        return 0;
    }

    int binary(int minPrec, bool noIn)
    {
        const int start = tok.start;
        int left;
        if (tok.t == Tk::Private) {   // #x in obj
            left = mk(PrivName, start);
            N[left].str = tokText(tok);
            next();
            fin(left);
            if (!isName("in")) throw ParseError { tok.start };
        } else {
            left = unary();
        }
        if (isArrow(left)) return left;
        for (;;) {
            const int prec = precedence(noIn);
            if (prec == 0 || prec < minPrec) break;
            const bool pow = is("**");
            const QString op = tokText(tok);
            next();
            const int inEnd = prevEnd;
            const int right = binary(pow ? prec : prec + 1, noIn);
            const bool priv = N[left].t == PrivName;
            const int id = mk(priv ? PrivIn : Binary, start);
            N[id].a = left;
            N[id].b = right;
            N[id].x0 = inEnd;
            N[id].str = op;
            left = fin(id);
        }
        if (N[left].t == PrivName) throw ParseError { N[left].start };
        return left;
    }

    int unary()
    {
        const int start = tok.start;
        if (tok.t == Tk::Punct && (is("!") || is("~") || is("+") || is("-") || is("++") || is("--"))) {
            const bool update = is("++") || is("--");
            next();
            const int arg = unary();
            const int id = mk(update ? Update : Unary, start);
            N[id].a = arg;
            return fin(id);
        }
        if (isName("typeof") || isName("void") || isName("delete")) {
            next();
            const int arg = unary();
            const int id = mk(Unary, start);
            N[id].a = arg;
            return fin(id);
        }
        if (isName("await")) {
            next();
            const int arg = unary();
            const int id = mk(Await, start);
            N[id].a = arg;
            return fin(id);
        }
        const int e = lhs(false);
        if (isArrow(e)) return e;
        if ((is("++") || is("--")) && !tok.nl) {
            next();
            const int id = mk(Update, start);
            N[id].a = e;
            return fin(id);
        }
        return e;
    }

    QVector<int> arguments(int id)
    {
        N[id].x0 = tok.start;
        expect("(");
        QVector<int> args;
        while (!is(")")) {
            if (is("...")) {
                const int s = mk(Spread, tok.start);
                N[s].x0 = tok.start;
                next();
                N[s].a = assign(false);
                args << fin(s);
            } else {
                args << assign(false);
            }
            if (!is(")")) {
                N[id].seps << tok.start;
                expect(",");
            }
        }
        N[id].x1 = tok.start;
        next();
        return args;
    }

    int lhs(bool noCall)
    {
        const int start = tok.start;
        int e;
        if (isName("new")) {
            next();
            if (is(".")) {
                next();
                if (!isName("target")) throw ParseError { tok.start };
                next();
                e = mk(MetaProp, start);
                N[e].str = QStringLiteral("new.target");
                fin(e);
            } else {
                const int callee = lhs(true);
                e = mk(New, start);
                N[e].a = callee;
                if (is("(")) {
                    const QVector<int> args = arguments(e);
                    N[e].list = args;
                }
                fin(e);
            }
        } else if (isName("super")) {
            next();
            e = fin(mk(Super, start));
        } else if (isName("import")) {
            next();
            if (is(".")) {
                next();
                if (!isName("meta")) throw ParseError { tok.start };
                next();
                e = mk(MetaProp, start);
                N[e].str = QStringLiteral("import.meta");
                fin(e);
            } else {
                e = mk(ImportCall, start);
                const QVector<int> args = arguments(e);
                N[e].list = args;
                fin(e);
            }
        } else {
            e = primary();
        }
        if (isArrow(e)) return e;
        const bool asyncCallee = N[e].t == Ident && N[e].str == QLatin1String("async") && is("(") && !tok.nl;
        bool firstCall = true, chain = false;
        for (;;) {
            if (is(".") || is("?.")) {
                const bool opt = is("?.");
                const int dot = tok.start;
                next();
                if (opt && (is("(") || is("["))) {
                    if (is("(")) {
                        if (noCall) throw ParseError { tok.start };
                        const int id = mk(Call, start);
                        N[id].a = e;
                        N[id].flags |= F_OPTIONAL;
                        N[id].x2 = dot;
                        const QVector<int> args = arguments(id);
                        N[id].list = args;
                        e = fin(id);
                    } else {
                        const int id = mk(Member, start);
                        N[id].flags |= F_COMPUTED | F_OPTIONAL;
                        N[id].x0 = dot;
                        N[id].x1 = tok.start;
                        next();
                        const int prop = expression(false);
                        N[id].x2 = tok.start;
                        expect("]");
                        N[id].a = e;
                        N[id].b = prop;
                        e = fin(id);
                    }
                } else {
                    int prop;
                    if (tok.t == Tk::Private) {
                        prop = mk(PrivName, tok.start);
                        N[prop].str = tokText(tok);
                    } else if (tok.t == Tk::Name) {
                        prop = mk(Ident, tok.start);
                        N[prop].str = tokText(tok);
                    } else {
                        throw ParseError { tok.start };
                    }
                    next();
                    fin(prop);
                    const int id = mk(Member, start);
                    if (opt) N[id].flags |= F_OPTIONAL;
                    N[id].x0 = dot;
                    N[id].a = e;
                    N[id].b = prop;
                    e = fin(id);
                }
                chain = chain || opt;
            } else if (is("[")) {
                const int id = mk(Member, start);
                N[id].flags |= F_COMPUTED;
                N[id].x1 = tok.start;
                next();
                const int prop = expression(false);
                N[id].x2 = tok.start;
                expect("]");
                N[id].a = e;
                N[id].b = prop;
                e = fin(id);
            } else if (is("(") && !noCall) {
                const int id = mk(Call, start);
                N[id].a = e;
                const QVector<int> args = arguments(id);
                N[id].list = args;
                e = fin(id);
                if (firstCall && asyncCallee && is("=>") && !tok.nl) {
                    QVector<int> params;
                    for (int arg : args) params << (N[arg].t == Spread ? (N[arg].t = Rest, N[arg].a = toPattern(N[arg].a), arg)
                                                                       : toPattern(arg));
                    return arrow(start, params, F_ASYNC, start, N[id].x0, N[id].x1 + 1);
                }
                firstCall = false;
            } else if (tok.t == Tk::Template) {
                if (chain) throw ParseError { tok.start };
                const int id = mk(Tagged, start);
                N[id].a = e;
                N[id].b = templateLit();
                e = fin(id);
            } else {
                break;
            }
        }
        return e;
    }

    int templateLit()
    {
        const int id = mk(Template, tok.start);
        QVector<int> exprs;
        for (;;) {
            if (tok.t != Tk::Template) throw ParseError { tok.start };
            const bool tail = tok.tail;
            next();
            if (tail) break;
            exprs << expression(false);
            if (!is("}")) throw ParseError { tok.start };
            tok = lx.templ(tok.start, tok.nl);
        }
        N[id].list = exprs;
        return fin(id);
    }

    int primary()
    {
        const int start = tok.start;
        switch (tok.t) {
        case Tk::Name: {
            if (isName("function")) return function(start, 0, -1);
            if (isName("class")) return classDef(false, start);
            if (isName("async")) {
                const Tok pk = peek();
                if (!pk.nl && pk.t == Tk::Name && eq(pk, "function")) {
                    next();
                    return function(start, F_ASYNC, start);
                }
            }
            if (isName("this")) {
                next();
                return fin(mk(This, start));
            }
            if (isName("null") || isName("true") || isName("false")) {
                next();
                return fin(mk(Lit, start));
            }
            if (reserved(tok) && !isName("let") && !isName("static") && !isName("yield")) throw ParseError { start };
            return ident();
        }
        case Tk::Num: next(); return fin(mk(Lit, start));
        case Tk::Str: {
            next();
            const int id = mk(Lit, start);
            N[id].flags |= F_STRING;
            return fin(id);
        }
        case Tk::BigInt: next(); return fin(mk(BigIntLit, start));
        case Tk::Template: return templateLit();
        case Tk::Punct:
            if (is("(")) return parenOrArrow();
            if (is("[")) return arrayLit();
            if (is("{")) return objectLit();
            if (is("/") || is("/=")) {
                tok = lx.regex(tok.start, tok.nl);
                const int id = mk(RegexLit, start);
                N[id].x0 = start + 1;
                int close = tok.end - 1;
                while (src[close] != QLatin1Char('/')) close--;
                N[id].x1 = close;
                N[id].x2 = tok.end;
                next();
                return fin(id);
            }
            break;
        default: break;
        }
        throw ParseError { start };
    }

    int parenOrArrow()
    {
        const int start = tok.start;
        next();
        QVector<int> items;
        bool rest = false, trailing = false;
        while (!is(")")) {
            if (is("...")) {
                const int r = mk(Rest, tok.start);
                N[r].x0 = tok.start;
                next();
                N[r].a = bindingTarget();
                items << fin(r);
                rest = true;
            } else {
                items << assign(false);
            }
            if (is(",")) {
                next();
                if (is(")")) trailing = true;
            } else if (!is(")")) {
                throw ParseError { tok.start };
            }
        }
        next();
        const int close = prevEnd;
        if (is("=>") && !tok.nl) {
            QVector<int> params;
            for (int it : items) params << toPattern(it);
            return arrow(start, params, 0, -1, start, close);
        }
        if (items.isEmpty() || rest || trailing) throw ParseError { start };
        int inner = items[0];
        if (items.size() > 1) {
            inner = mk(Seq, N[items[0]].start);
            N[inner].list = items;
            N[inner].end = N[items.last()].end;
        }
        const int id = mk(Paren, start);
        N[id].a = inner;
        return fin(id);
    }

    int arrayLit()
    {
        const int id = mk(ArrayX, tok.start);
        next();
        QVector<int> items;
        while (!is("]")) {
            if (is(",")) {
                items << -1;
                N[id].seps << tok.start;
                next();
                continue;
            }
            if (is("...")) {
                const int s = mk(Spread, tok.start);
                N[s].x0 = tok.start;
                next();
                N[s].a = assign(false);
                items << fin(s);
            } else {
                items << assign(false);
            }
            if (is(",")) {
                N[id].seps << tok.start;
                next();
            } else if (!is("]")) {
                throw ParseError { tok.start };
            }
        }
        next();
        N[id].list = items;
        return fin(id);
    }

    int objectLit()
    {
        const int id = mk(ObjectX, tok.start);
        next();
        QVector<int> props;
        while (!is("}")) {
            if (is("...")) {
                const int s = mk(Spread, tok.start);
                N[s].x0 = tok.start;
                next();
                N[s].a = assign(false);
                props << fin(s);
            } else {
                props << property();
            }
            if (is(",")) {
                N[id].seps << tok.start;
                next();
            } else if (!is("}")) {
                throw ParseError { tok.start };
            }
        }
        next();
        N[id].list = props;
        return fin(id);
    }

    int property()
    {
        const int ps = tok.start;
        int flags = 0, asyncPos = -1;
        if (isName("async")) {
            const Tok pk = peek();
            if (!elementEnd(pk) && !pk.nl) {
                flags |= F_ASYNC;
                asyncPos = tok.start;
                next();
            }
        }
        int star = -1;
        if (is("*")) {
            flags |= F_GEN;
            star = tok.start;
            next();
        }
        if ((isName("get") || isName("set")) && !(flags & (F_ASYNC | F_GEN))) {
            const Tok pk = peek();
            if (!elementEnd(pk)) {
                flags |= isName("get") ? F_GET : F_SET;
                next();
            }
        }
        const int ks = tok.start;
        bool computed = false;
        const int key = propertyKey(computed);
        const int ke = prevEnd;
        const int id = mk(Prop, ps);
        N[id].x1 = ks;
        N[id].x2 = ke;
        N[id].a = key;
        if (computed) flags |= F_COMPUTED;
        if (is("(")) {
            const int fn = mk(Func, tok.start);
            N[fn].flags = flags & (F_ASYNC | F_GEN);
            N[fn].x0 = asyncPos;
            N[fn].d = star;
            paramsAndBody(fn);
            fin(fn);
            N[id].b = fn;
            N[id].flags = flags | F_METHOD;
            return fin(id);
        }
        if (flags & (F_ASYNC | F_GEN | F_GET | F_SET)) throw ParseError { tok.start };
        if (is(":")) {
            next();
            N[id].b = assign(false);
        } else {
            if (computed || N[key].t != Ident) throw ParseError { tok.start };
            flags |= F_SHORTHAND;
            if (is("=")) {   // { a = 1 }: only valid once it turns into a pattern
                const int as = mk(Assign, ks);
                N[as].x0 = tok.start;
                next();
                N[as].a = key;
                N[as].str = QStringLiteral("=");
                const int def = assign(false);
                N[as].b = def;
                N[id].b = fin(as);
            } else {
                N[id].b = key;
            }
        }
        N[id].flags = flags;
        return fin(id);
    }
};

// Children of a node, in source order.
QVector<int> kids(const QVector<Node> &N, int id)
{
    const Node &n = N[id];
    QVector<int> out;
    auto add = [&](int k) {
        if (k >= 0) out << k;
    };
    switch (n.t) {
    case Func:
        add(n.a);
        for (int k : n.list) add(k);
        add(n.b);
        break;
    case Field:   // c and d are offsets
        add(n.a);
        add(n.b);
        break;
    case ForIn:   // d is an offset
    case ForOf:
        add(n.a);
        add(n.b);
        add(n.c);
        break;
    case Prop:    // a shorthand property's value is its key
        add(n.a);
        if (n.b != n.a) add(n.b);
        break;
    case Class:
        add(n.a);
        add(n.b);
        for (int k : n.list) add(k);
        break;
    default:
        add(n.a);
        add(n.b);
        add(n.c);
        add(n.d);
        for (int k : n.list) add(k);
    }
    return out;
}

// ---------------------------------------------------------------- edits

class Rewriter
{
public:
    explicit Rewriter(const QString &src) : src(src) { }
    const QString &src;

    void replace(int pos, int len, const QString &text) { add(pos, len, text, 0); }
    // Openers at a position go outer first, closers inner first, and closers before openers
    // (the end of one construct can be the start of the next).
    void before(int pos, const QString &text, int depth) { add(pos, 0, text, -100000 + depth); }
    void after(int pos, const QString &text, int depth) { add(pos, 0, text, -300000 - depth); }
    bool empty() const { return edits.isEmpty(); }

    QString apply(bool &ok) const
    {
        QVector<Edit> e = edits;
        std::stable_sort(e.begin(), e.end(), [](const Edit &x, const Edit &y) {
            return x.pos != y.pos ? x.pos < y.pos : x.order < y.order;
        });
        QString out;
        out.reserve(src.size() + 256);
        int cursor = 0;
        ok = true;
        for (const Edit &x : e) {
            if (x.pos < cursor) {
                ok = false;
                return src;
            }
            out += QStringView(src).mid(cursor, x.pos - cursor);
            out += x.text;
            cursor = x.pos + x.len;
        }
        out += QStringView(src).mid(cursor);
        return out;
    }

private:
    struct Edit {
        int pos, len;
        QString text;
        int order;
    };
    QVector<Edit> edits;
    void add(int pos, int len, const QString &text, int order) { edits.push_back({ pos, len, text, order }); }
};

int lineOf(const QString &s, int offset)
{
    int line = 1;
    for (int i = 0; i < offset && i < s.size(); i++) {
        const QChar c = s[i];
        if (c == QLatin1Char('\r')) {
            if (i + 1 < s.size() && s[i + 1] == QLatin1Char('\n')) i++;
            line++;
        } else if (isLineTerm(c)) {
            line++;
        }
    }
    return line;
}

// Where generated declarations go: the start of the module, after a hashbang line.
int prologuePos(const QString &s)
{
    if (!s.startsWith(QLatin1String("#!"))) return 0;
    int p = 0;
    while (p < s.size() && !isLineTerm(s[p])) p++;
    if (p < s.size() && s[p] == QLatin1Char('\r') && p + 1 < s.size() && s[p + 1] == QLatin1Char('\n')) p++;
    return std::min(p + 1, int(s.size()));
}

QString quote(const QString &s)
{
    QString out = QStringLiteral("\"");
    for (QChar c : s) {
        const ushort u = c.unicode();
        if (c == QLatin1Char('\\') || c == QLatin1Char('"')) out += QLatin1Char('\\') + QString(c);
        else if (u == '\n') out += QStringLiteral("\\n");
        else if (u == '\r') out += QStringLiteral("\\r");
        else if (u == 0x2028 || u == 0x2029) out += QStringLiteral("\\u%1").arg(uint(u), 4, 16, QLatin1Char('0'));
        else out += c;
    }
    return out + QLatin1Char('"');
}

struct Unsupported {
    QString message;
};

// State shared by the passes over one module: fresh names never repeat between passes.
struct Module {
    QString url;
    int counter = 0;
    QString name(const char *prefix) { return QStringLiteral("__qtm_%1%2").arg(QLatin1String(prefix)).arg(++counter); }
};

// Base of a pass: the tree, the edits, and a parent map.
struct Pass {
    Pass(Parser &p, Module &m) : P(p), N(p.N), R(p.src), M(m), src(p.src)
    {
        parent.fill(-1, N.size());
        for (int i = 0; i < N.size(); i++)
            for (int k : kids(N, i)) parent[k] = i;
    }
    Parser &P;
    QVector<Node> &N;
    Rewriter R;
    Module &M;
    const QString &src;
    QVector<int> parent;
    QStringList prologue;   // declarations for the start of the module

    QString text(int id) const { return src.mid(N[id].start, N[id].end - N[id].start); }
    QString text(int from, int to) const { return src.mid(from, to - from); }
    void finishPrologue()
    {
        if (!prologue.isEmpty()) R.before(prologuePos(src), prologue.join(QString()), -1);
    }
    bool isPattern(int id) const { return id >= 0 && (N[id].t == ObjectPat || N[id].t == ArrayPat); }
    bool containsObjectRest(int id) const
    {
        if (id < 0) return false;
        switch (N[id].t) {
        case ObjectPat:
            for (int p : N[id].list) {
                if (N[p].t == Rest) return true;
                if (N[p].t == Prop && containsObjectRest(N[p].b)) return true;
            }
            return false;
        case ArrayPat:
            for (int e : N[id].list)
                if (containsObjectRest(e)) return true;
            return false;
        case AssignPat: return containsObjectRest(N[id].a);
        case Rest: return containsObjectRest(N[id].a);
        default: return false;
        }
    }
    // A destructuring parameter: a pattern, with or without a default, or a rest pattern.
    bool isPatternParam(int id) const
    {
        if (N[id].t == AssignPat || N[id].t == Rest) return isPattern(N[id].a);
        return isPattern(id);
    }
};

// ---------------------------------------------------------------- pass 1: checks and small rewrites

struct CheckPass : Pass {
    using Pass::Pass;
    int fnDepth = 0;

    void unsupported(const QString &what, int offset)
    {
        throw Unsupported { QStringLiteral("%1 is not supported by this app's JavaScript engine (line %2)")
                                .arg(what)
                                .arg(lineOf(src, offset)) };
    }

    void run()
    {
        walk(P.root);
        finishPrologue();
    }

    void regex(int id)
    {
        const Node &n = N[id];
        const QString pattern = text(n.x0, n.x1), flags = text(n.x1 + 1, n.x2);
        for (const char *f : { "d", "v" })
            if (flags.contains(QLatin1String(f)))
                unsupported(QStringLiteral("The regular expression flag '%1'").arg(QLatin1String(f)), n.start);
        const bool unicode = flags.contains(QLatin1Char('u'));
        QSet<QString> names;
        bool cls = false;
        for (int i = 0; i < pattern.size(); i++) {
            const QChar c = pattern[i];
            if (c == QLatin1Char('\\')) {
                const QChar e = i + 1 < pattern.size() ? pattern[i + 1] : QChar();
                if (unicode && (e == QLatin1Char('p') || e == QLatin1Char('P')))
                    unsupported(QStringLiteral("Unicode property escapes in regular expressions (\\%1{...})").arg(e), n.start);
                i++;
                continue;
            }
            if (cls) {
                if (c == QLatin1Char(']')) cls = false;
                continue;
            }
            if (c == QLatin1Char('[')) {
                cls = true;
                continue;
            }
            if (c != QLatin1Char('(') || i + 1 >= pattern.size() || pattern[i + 1] != QLatin1Char('?')) continue;
            const QStringView rest = QStringView(pattern).mid(i + 2);
            if (rest.startsWith(u"<=") || rest.startsWith(u"<!"))
                unsupported(QStringLiteral("Regular expression lookbehind ((?<= ...) and (?<! ...))"), n.start);
            if (rest.startsWith(u"<")) {
                const int close = int(rest.indexOf(QLatin1Char('>')));
                const QString name = rest.mid(1, close - 1).toString();
                if (names.contains(name)) unsupported(QStringLiteral("A duplicate regular expression group name"), n.start);
                names.insert(name);
            } else if (!rest.startsWith(u":") && !rest.startsWith(u"=") && !rest.startsWith(u"!")) {
                unsupported(QStringLiteral("Regular expression modifiers ((?i: ...))"), n.start);
            }
        }
        if (!names.isEmpty() || flags.contains(QLatin1Char('s')))
            R.replace(n.start, n.end - n.start, QStringLiteral("__qtm.regex(%1, %2)").arg(quote(pattern), quote(flags)));
    }

    void walk(int id)
    {
        Node &n = N[id];
        switch (n.t) {
        case Func:
            fnDepth++;
            for (int k : kids(N, id)) walk(k);
            fnDepth--;
            return;
        case Field:
        case StaticBlock:
            // Their code runs in a function of its own (an initializer), never at the top level.
            fnDepth++;
            for (int k : kids(N, id)) walk(k);
            fnDepth--;
            return;
        case BigIntLit: unsupported(QStringLiteral("BigInt (%1)").arg(text(id)), n.start); break;
        case RegexLit: regex(id); break;
        case Await:
            if (fnDepth == 0) unsupported(QStringLiteral("Top-level await"), n.start);
            break;
        case ForOf:
            if ((n.flags & F_AWAIT) && fnDepth == 0) unsupported(QStringLiteral("Top-level await (for await)"), n.start);
            break;
        case Catch:
            if (n.a < 0) R.after(n.x0, QStringLiteral(" (__qtm_e)"), 0);
            break;
        case ExportDecl:
            if ((n.flags & F_STAR) && !n.str2.isEmpty()) {
                const QString ns = M.name("ns");
                R.replace(n.start, n.end - n.start,
                          QStringLiteral("import * as %1 from %2; export { %1 as %3 };").arg(ns, n.str, n.str2));
                return;
            }
            break;
        case MetaProp:
            if (n.str == QLatin1String("import.meta")) {
                if (prologue.isEmpty()) prologue << QStringLiteral("const __qtm_meta = __qtm.importMeta(%1);").arg(quote(M.url));
                R.replace(n.start, n.end - n.start, QStringLiteral("__qtm_meta"));
            }
            break;
        case ImportCall:
            R.replace(n.start, n.x0 + 1 - n.start, QStringLiteral("__qtm.importDynamic(%1, ").arg(quote(M.url)));
            break;
        case Lit:
            if (n.flags & F_STRING) {
                for (int i = n.start; i < n.end; i++)
                    if (src[i].unicode() == 0x2028 || src[i].unicode() == 0x2029)
                        R.replace(i, 1, QStringLiteral("\\u%1").arg(uint(src[i].unicode()), 4, 16, QLatin1Char('0')));
            }
            break;
        default: break;
        }
        for (int k : kids(N, id)) walk(k);
    }
};

// ---------------------------------------------------------------- pass 2: classes

struct ClassPass : Pass {
    using Pass::Pass;
    QStringList symbols, descs;
    bool usesSlots = false;
    QVector<QHash<QString, QString>> privates;   // the private names of the classes we are in

    void run()
    {
        walk(P.root, 0);
        QStringList decl = symbols;
        if (usesSlots) decl << QStringLiteral("__qtm_ck = []");
        decl << descs;
        if (!decl.isEmpty()) prologue << QStringLiteral("const %1;").arg(decl.join(QStringLiteral(", ")));
        finishPrologue();
    }

    QString privateSymbol(const QString &name) const
    {
        for (int i = privates.size() - 1; i >= 0; i--)
            if (privates[i].contains(name)) return privates[i].value(name);
        return QString();
    }

    // The name an anonymous class expression gets from where it stands (const X = class {}).
    QString inferredName(int id) const
    {
        const int p = parent[id];
        if (p < 0) return QString();
        const Node &n = N[p];
        auto keyName = [&](int key, int flags) -> QString {
            if (flags & F_COMPUTED || key < 0) return QString();
            if (N[key].t == Ident) return N[key].str;
            if (N[key].t == Lit && (N[key].flags & F_STRING)) return text(key).mid(1, text(key).size() - 2);
            return QString();
        };
        if ((n.t == Declarator || n.t == AssignPat) && n.b == id && N[n.a].t == Ident) return N[n.a].str;
        if (n.t == Assign && n.b == id && n.str == QLatin1String("=") && N[n.a].t == Ident) return N[n.a].str;
        if ((n.t == Prop || n.t == Field) && n.b == id) return keyName(n.a, n.flags);
        if (n.t == ExportDecl && (n.flags & F_DEFAULT)) return QStringLiteral("default");
        return QString();
    }

    // super(...) calls of a derived constructor (not those of classes nested in it).
    void superCalls(int id, QVector<int> &out) const
    {
        const Node &n = N[id];
        if (n.t == Class || (n.t == Func && !(n.flags & F_ARROW))) return;
        if (n.t == Call && n.a >= 0 && N[n.a].t == Super) out << id;
        for (int k : kids(N, id)) superCalls(k, out);
    }

    void classDef(int id, int depth)
    {
        const Node &c = N[id];
        bool fields = false, instance = false;
        int ctor = -1;
        for (int e : c.list) {
            if (N[e].t == Field || N[e].t == StaticBlock) fields = true;
            if (N[e].t == Field && !(N[e].flags & F_STATIC)) instance = true;
            if (N[e].t == Method && (N[e].flags & F_CTOR)) ctor = e;
        }
        // Private methods and accessors: symbol-keyed members.
        for (int e : c.list)
            if (N[e].t == Method && N[N[e].a].t == PrivName)
                R.replace(N[e].x1, N[e].x2 - N[e].x1, QStringLiteral("[%1]").arg(privateSymbol(N[N[e].a].str)));
        if (!fields) return;

        QString name = c.a >= 0 ? N[c.a].str : QString();
        const bool anonymous = name.isEmpty();
        if (anonymous && instance) {
            name = M.name("c");
            R.after(c.x2, QLatin1Char(' ') + name, depth);
        }
        QStringList entries;
        for (int e : c.list) {
            const Node &el = N[e];
            if (el.t == StaticBlock) {
                const QString f = M.name("f");
                symbols << f + QStringLiteral(" = Symbol()");
                R.replace(el.x0, N[el.a].start + 1 - el.x0, QStringLiteral("static [%1]() {").arg(f));
                entries << QStringLiteral("[2, null, %1]").arg(f);
                continue;
            }
            if (el.t != Field) continue;
            const QString f = M.name("f");
            symbols << f + QStringLiteral(" = Symbol()");
            const Node &key = N[el.a];
            QString keyExpr, extra;
            if (el.flags & F_COMPUTED) {
                const int slot = M.counter++;
                usesSlots = true;
                keyExpr = QStringLiteral("() => __qtm_ck[%1]").arg(slot);
                R.replace(el.x1, 1, QStringLiteral("[(__qtm_ck[%1] = __qtm.key(").arg(slot));
                R.replace(el.x2 - 1, 1, QStringLiteral("), %1)]").arg(f));
            } else {
                if (key.t == PrivName) {
                    keyExpr = privateSymbol(key.str);
                    extra = QStringLiteral(", %1").arg(quote(key.str));
                } else if (key.t == Ident) {
                    keyExpr = quote(key.str);
                } else if (key.flags & F_STRING) {
                    keyExpr = text(el.a);
                } else {
                    keyExpr = QStringLiteral("String(%1)").arg(text(el.a));
                }
                R.replace(el.x1, el.x2 - el.x1, QStringLiteral("[%1]").arg(f));
            }
            if (el.b >= 0) {
                R.replace(el.d, 1, QStringLiteral("() { return ("));
                R.after(N[el.b].end, QStringLiteral("); }"), depth);
            } else {
                R.after(el.x2, QStringLiteral("() {}"), depth);
            }
            entries << QStringLiteral("[%1, %2, %3%4]").arg((el.flags & F_STATIC) ? 1 : 0).arg(keyExpr, f, extra);
        }
        const QString desc = M.name("d");
        descs << QStringLiteral("%1 = [%2]").arg(desc, entries.join(QStringLiteral(", ")));

        if (instance) {
            const QString init = QStringLiteral("__qtm.initFields(this, %1)").arg(name);
            if (ctor >= 0) {
                const int fn = N[ctor].b;
                if (c.b >= 0) {
                    QVector<int> calls;
                    superCalls(N[fn].b, calls);
                    for (int call : calls) {
                        R.before(N[call].start, QStringLiteral("("), depth);
                        R.after(N[call].end, QStringLiteral(", %1, this)").arg(init), depth);
                    }
                } else {
                    R.after(N[N[fn].b].start + 1, QStringLiteral(" %1;").arg(init), depth);
                }
            } else {
                R.after(c.x0 + 1,
                        c.b >= 0 ? QStringLiteral(" constructor(...__qtm_a) { super(...__qtm_a); %1; }").arg(init)
                                 : QStringLiteral(" constructor() { %1; }").arg(init),
                        depth);
            }
        }
        if ((c.flags & F_DECL) && !anonymous) {
            R.after(c.end, QStringLiteral(" __qtm.defineClass(%1, %2);").arg(name, desc), depth);
        } else {
            R.before(c.start, QStringLiteral("__qtm.defineClass("), depth);
            R.after(c.end,
                    anonymous ? QStringLiteral(", %1, %2)").arg(desc, quote(inferredName(id))) : QStringLiteral(", %1)").arg(desc),
                    depth);
        }
    }

    void walk(int id, int depth)
    {
        const Node &n = N[id];
        if (n.t == Class) {
            if (n.a >= 0) walk(n.a, depth + 1);
            if (n.b >= 0) walk(n.b, depth + 1);
            QHash<QString, QString> names;
            for (int e : n.list) {
                const int key = (N[e].t == Method || N[e].t == Field) ? N[e].a : -1;
                if (key >= 0 && N[key].t == PrivName && !names.contains(N[key].str)) {
                    const QString sym = M.name("p");
                    names.insert(N[key].str, sym);
                    symbols << QStringLiteral("%1 = __qtm.privateName(%2)").arg(sym, quote(N[key].str));
                }
            }
            privates.push_back(names);
            classDef(id, depth);
            for (int e : n.list) {
                if (N[e].t == StaticBlock) {
                    walk(N[e].a, depth + 1);
                    continue;
                }
                if (N[e].flags & F_COMPUTED) walk(N[e].a, depth + 1);
                if (N[e].b >= 0) walk(N[e].b, depth + 1);
            }
            privates.pop_back();
            return;
        }
        if (n.t == Member && n.b >= 0 && N[n.b].t == PrivName) {
            const QString sym = privateSymbol(N[n.b].str);
            if (!sym.isEmpty())
                R.replace(n.x0, N[n.b].end - n.x0, QStringLiteral("%1[%2]").arg((n.flags & F_OPTIONAL) ? QStringLiteral("?.") : QString(), sym));
            walk(n.a, depth + 1);
            return;
        }
        if (n.t == PrivIn) {
            const QString sym = privateSymbol(N[n.a].str);
            if (!sym.isEmpty()) {
                R.replace(n.start, n.x0 - n.start, QStringLiteral("__qtm.privateIn(%1,").arg(sym));
                R.after(N[n.b].end, QStringLiteral(")"), depth);
            }
            walk(n.b, depth + 1);
            return;
        }
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- pass 3: lower destructuring parameters

// Parameters V4 can't bind (object rest, or two or more patterns: a V4 bug binds them all to the
// last pattern's argument) become plain names, destructured by a `var` at the start of the body.
// for-in/of heads and catch parameters with object rest move into their body the same way. The
// moved patterns are then handled by the object rest pass.
struct LowerPass : Pass {
    using Pass::Pass;

    void run() { walk(P.root, 0); }

    void bodyPrologue(int fn, const QString &code, int depth)
    {
        const Node &f = N[fn];
        if (f.flags & F_CONCISE) {
            R.before(N[f.b].start, QStringLiteral("{%1 return (").arg(code), depth);
            R.after(N[f.b].end, QStringLiteral("); }"), depth);
        } else {
            R.after(N[f.b].start + 1, code, depth);
        }
    }

    // Wraps a loop body: { <code> BODY }
    void wrapBody(int body, const QString &code, int depth)
    {
        R.before(N[body].start, QStringLiteral("{ %1 ").arg(code), depth);
        R.after(N[body].end, QStringLiteral(" }"), depth);
    }

    bool lowerParams(int fn, int depth)
    {
        const Node &f = N[fn];
        int patterns = 0, first = -1;
        bool rest = false;
        for (int i = 0; i < f.list.size(); i++) {
            const int p = f.list[i];
            if (isPatternParam(p)) {
                patterns++;
                if (first < 0) first = i;
            }
            if (containsObjectRest(p)) rest = true;
        }
        if (patterns < 2 && !rest) return false;
        QStringList decls;
        for (int i = first; i < f.list.size(); i++) {
            const int p = f.list[i];
            const Node &pn = N[p];
            if (pn.t == Rest) {
                if (N[pn.a].t == Ident) continue;
                const QString t = M.name("a");
                R.replace(N[pn.a].start, N[pn.a].end - N[pn.a].start, t);
                decls << QStringLiteral("%1 = %2").arg(text(pn.a), t);
            } else if (pn.t == AssignPat) {
                const QString t = M.name("a");
                R.replace(pn.start, pn.end - pn.start, t + QStringLiteral(" = undefined"));
                decls << QStringLiteral("%1 = %2 === undefined ? (%3) : %2").arg(text(pn.a), t, text(pn.b));
            } else {
                const QString t = M.name("a");
                R.replace(pn.start, pn.end - pn.start, t);
                decls << QStringLiteral("%1 = %2").arg(text(p), t);
            }
        }
        bodyPrologue(fn, QStringLiteral(" var %1;").arg(decls.join(QStringLiteral(", "))), depth);
        return true;
    }

    void walk(int id, int depth)
    {
        const Node &n = N[id];
        switch (n.t) {
        case Func:
            if (lowerParams(id, depth)) {
                if (n.a >= 0) walk(n.a, depth + 1);
                walk(n.b, depth + 1);
                return;
            }
            break;
        case Catch:
            if (n.a >= 0 && containsObjectRest(n.a)) {
                const QString t = M.name("e");
                R.replace(N[n.a].start, N[n.a].end - N[n.a].start, t);
                R.after(N[n.b].start + 1, QStringLiteral(" let %1 = %2;").arg(text(n.a), t), depth);
                walk(n.b, depth + 1);
                return;
            }
            break;
        case ForIn:
        case ForOf: {
            const int left = n.a;
            const bool decl = N[left].t == VarDecl;
            const int pat = decl ? N[N[left].list[0]].a : left;
            if (containsObjectRest(pat)) {
                const QString t = M.name("f");
                if (decl) {
                    R.replace(N[pat].start, N[pat].end - N[pat].start, t);
                    wrapBody(n.c, QStringLiteral("%1 %2 = %3;").arg(N[left].str, text(pat), t), depth);
                } else {
                    R.replace(N[pat].start, N[pat].end - N[pat].start, QStringLiteral("const ") + t);
                    wrapBody(n.c, QStringLiteral("(%1 = %2);").arg(text(pat), t), depth);
                }
                walk(n.b, depth + 1);
                walk(n.c, depth + 1);
                return;
            }
            break;
        }
        default: break;
        }
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- temporaries

// Variables a rewrite needs (to evaluate something once): declared with `var` at the start of the
// innermost function (a concise arrow body becomes a block for it), or of the module.
struct TempPass : Pass {
    using Pass::Pass;
    QVector<int> owners;                 // innermost function last; -1 = module
    QHash<int, QStringList> temps;
    QHash<int, int> ownerDepth;

    QString temp(const char *prefix)
    {
        const QString t = M.name(prefix);
        temps[owners.isEmpty() ? -1 : owners.last()] << t;
        return t;
    }
    void enter(int fn, int depth)
    {
        owners << fn;
        ownerDepth[fn] = depth;
    }
    void finishTemps()
    {
        for (auto it = temps.begin(); it != temps.end(); ++it) {
            const QString decl = QStringLiteral("var %1;").arg(it.value().join(QStringLiteral(", ")));
            if (it.key() < 0) {
                prologue << decl;
                continue;
            }
            const Node &f = N[it.key()];
            const int depth = ownerDepth.value(it.key());
            if (f.flags & F_CONCISE) {
                R.before(N[f.b].start, QStringLiteral("{ %1 return (").arg(decl), depth);
                R.after(N[f.b].end, QStringLiteral("); }"), depth);
            } else {
                R.after(N[f.b].start + 1, QLatin1Char(' ') + decl, depth);
            }
        }
        finishPrologue();
    }
};

// ---------------------------------------------------------------- pass 4: object rest in patterns

// const { a, [k]: b, c: { d, ...e }, ...f } = o  becomes
// const __t1 = o, __k2 = (__qtm.coerce(__t1), __qtm.key(k)), { a, [__k2]: b, c: __t3 } = __t1, { d } = __t3,
//       e = __qtm.rest(__t3, ["d"]), f = __qtm.rest(__t1, ["a", __k2, "c"])
// Properties are read in the original order. Assignments become a comma expression on temporaries.
struct RestPass : TempPass {
    using TempPass::TempPass;
    bool declMode = true;

    QString newTemp(const char *prefix) { return declMode ? M.name(prefix) : temp(prefix); }

    void flatten(int pat, const QString &src, QStringList &out)
    {
        const Node &p = N[pat];
        if (p.t == ArrayPat) {
            QStringList elems;
            QVector<QPair<int, QString>> nested;
            for (int el : p.list) {
                if (el < 0) {
                    elems << QString();
                    continue;
                }
                if (!containsObjectRest(el)) {
                    elems << text(el);
                    continue;
                }
                const QString t = newTemp("t");
                const Node &e = N[el];
                if (e.t == Rest) elems << QStringLiteral("...") + t;
                else if (e.t == AssignPat) elems << t + QStringLiteral(" = ") + text(e.b);
                else elems << t;
                nested << qMakePair(e.t == Rest || e.t == AssignPat ? e.a : el, t);
            }
            if (!p.list.isEmpty() && p.list.last() < 0) elems << QString();
            out << QStringLiteral("[%1] = %2").arg(elems.join(QStringLiteral(", ")), src);
            for (const auto &x : nested) flatten(x.first, x.second, out);
            return;
        }
        QStringList segment, keys;
        // The first use of the source checks it first, with V8's message naming the first property.
        bool first = true;
        QString firstKey;
        if (!p.list.isEmpty() && N[p.list[0]].t == Prop && !(N[p.list[0]].flags & F_COMPUTED)) {
            const int k = N[p.list[0]].a;
            if (N[k].t == Ident) firstKey = N[k].str;
            else if (N[k].flags & F_STRING) firstKey = text(k).mid(1, text(k).size() - 2);
        }
        auto source = [&]() {
            if (!first) return src;
            first = false;
            return QStringLiteral("__qtm.coerce(%1%2)").arg(src, firstKey.isEmpty() ? QString() : QStringLiteral(", ") + quote(firstKey));
        };
        auto flush = [&]() {
            if (segment.isEmpty()) return;
            out << QStringLiteral("{ %1 } = %2").arg(segment.join(QStringLiteral(", ")), source());
            segment.clear();
        };
        for (int prop : p.list) {
            const Node &pr = N[prop];
            if (pr.t == Rest) {
                flush();
                out << QStringLiteral("%1 = __qtm.rest(%2, [%3])").arg(text(pr.a), source(), keys.join(QStringLiteral(", ")));
                continue;
            }
            QString keyRef;
            if (pr.flags & F_COMPUTED) {
                const QString k = newTemp("k");
                flush();
                const bool check = first;
                const QString checked = source();
                out << QStringLiteral("%1 = (%2__qtm.key(%3))").arg(k, check ? checked + QStringLiteral(", ") : QString(), text(pr.a));
                keyRef = QLatin1Char('[') + k + QLatin1Char(']');
                keys << k;
            } else {
                keyRef = text(pr.a);
                keys << (N[pr.a].t == Ident ? quote(N[pr.a].str) : text(pr.a));
            }
            const int value = pr.b;
            if (containsObjectRest(value)) {
                const QString t = newTemp("t");
                const bool def = N[value].t == AssignPat;
                segment << QStringLiteral("%1: %2%3").arg(keyRef, t, def ? QStringLiteral(" = ") + text(N[value].b) : QString());
                flush();
                flatten(def ? N[value].a : value, t, out);
            } else if ((pr.flags & F_SHORTHAND) && !(pr.flags & F_COMPUTED)) {
                segment << text(prop);
            } else {
                segment << keyRef + QStringLiteral(": ") + text(value);
            }
        }
        flush();
    }

    bool changedAny = false;

    void run()
    {
        walk(P.root, 0);
        finishTemps();
    }

    void walk(int id, int depth)
    {
        const Node &n = N[id];
        if (n.t == Func) {
            enter(id, depth);
            for (int k : kids(N, id)) walk(k, depth + 1);
            owners.pop_back();
            return;
        }
        if (n.t == VarDecl) {
            for (int d : n.list) {
                const Node &dn = N[d];
                if (dn.b >= 0 && containsObjectRest(dn.a)) {
                    declMode = true;
                    const QString t = M.name("t");
                    QStringList out;
                    flatten(dn.a, t, out);
                    R.replace(N[dn.a].start, N[dn.a].end - N[dn.a].start, t);
                    R.after(N[dn.b].end, QStringLiteral(", ") + out.join(QStringLiteral(", ")), depth);
                    walk(dn.b, depth + 1);
                } else {
                    for (int k : kids(N, d)) walk(k, depth + 1);
                }
            }
            return;
        }
        if (n.t == Assign && n.str == QLatin1String("=") && isPattern(n.a) && containsObjectRest(n.a)) {
            declMode = false;
            const QString t = temp("t");
            QStringList out;
            flatten(n.a, t, out);
            R.replace(N[n.a].start, N[n.a].end - N[n.a].start, QLatin1Char('(') + t);
            R.after(N[n.b].end, QStringLiteral(", %1, %2)").arg(out.join(QStringLiteral(", ")), t), depth);
            walk(n.b, depth + 1);
            return;
        }
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- pass 5a: __proto__ in object literals

// V4 makes `__proto__: v` in an object literal an own property instead of the prototype. The key
// becomes a symbol (so the value is still evaluated in its place), and __qtm.protoLiteral sets the
// prototype from it once the literal is built.
struct ProtoPass : Pass {
    using Pass::Pass;

    void run() { walk(P.root, 0); }

    bool isProtoKey(int prop) const
    {
        const Node &p = N[prop];
        if (p.t != Prop || (p.flags & (F_COMPUTED | F_SHORTHAND | F_METHOD | F_GET | F_SET))) return false;
        const QString k = text(p.a);
        return k == QLatin1String("__proto__") || k == QLatin1String("\"__proto__\"") || k == QLatin1String("'__proto__'");
    }

    void walk(int id, int depth)
    {
        const Node &n = N[id];
        if (n.t == ObjectX) {
            for (int p : n.list)
                if (isProtoKey(p)) {
                    R.replace(N[p].x1, N[p].x2 - N[p].x1, QStringLiteral("[__qtm.PROTO]"));
                    R.before(n.start, QStringLiteral("__qtm.protoLiteral("), depth);
                    R.after(n.end, QStringLiteral(")"), depth);
                    break;
                }
        }
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- pass 5: object spread

// { a, ...x, b: 1, ...y }  becomes  __qtm.spreadProps(__qtm.defineProps(__qtm.spreadProps({ a }, x), { b: 1 }), y):
// the literal's own parts keep their exact meaning (accessors, methods, __proto__), each spread
// copies own enumerable properties in order, and everything is evaluated left to right.
struct SpreadPass : Pass {
    using Pass::Pass;

    void run() { walk(P.root, 0); }

    void object(int id, int depth)
    {
        const Node &o = N[id];
        const QVector<int> &items = o.list;
        bool any = false;
        for (int it : items) any = any || N[it].t == Spread;
        if (!any) return;
        auto isSpread = [&](int i) { return N[items[i]].t == Spread; };
        // Calls to open, in order of application.
        QStringList calls;
        bool seenSpread = false;
        for (int i = 0; i < items.size(); i++) {
            if (isSpread(i)) {
                calls << QStringLiteral("__qtm.spreadProps(");
                seenSpread = true;
            } else if (seenSpread && (i == 0 || isSpread(i - 1))) {
                calls << QStringLiteral("__qtm.defineProps(");
            }
        }
        QString open;
        for (int i = calls.size() - 1; i >= 0; i--) open += calls[i];
        R.replace(o.start, 1, open + (isSpread(0) ? QStringLiteral("{},") : QStringLiteral("{")));
        // Is item i in the first chunk (the literal that becomes the result)?
        int firstSpread = 0;
        while (!isSpread(firstSpread)) firstSpread++;
        for (int i = 0; i < items.size(); i++) {
            if (isSpread(i)) R.replace(N[items[i]].x0, 3, QString());
            if (i + 1 >= items.size()) break;
            const int sep = o.seps[i];
            if (isSpread(i + 1)) {
                if (isSpread(i)) R.replace(sep, 1, QStringLiteral("),"));
                else if (i < firstSpread) R.replace(sep, 1, QStringLiteral("},"));
                else R.replace(sep, 1, QStringLiteral("}),"));
            } else if (isSpread(i)) {
                R.replace(sep, 1, QStringLiteral("), {"));
            }
        }
        if (o.seps.size() == items.size()) R.replace(o.seps.last(), 1, QStringLiteral(" "));   // trailing comma
        R.replace(o.end - 1, 1, isSpread(int(items.size()) - 1) ? QStringLiteral(")") : QStringLiteral("})"));
        Q_UNUSED(depth);
    }

    void walk(int id, int depth)
    {
        if (N[id].t == ObjectX) object(id, depth);
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- pass 6: logical assignment

// a ??= b -> (a ?? (a = b));  o.p ||= v -> ((t = o).p || (t.p = v));  o[k] &&= v -> ((t = o)[u = key(k)] && (t[u] = v))
struct LogicalPass : TempPass {
    using TempPass::TempPass;

    void run()
    {
        walk(P.root, 0);
        finishTemps();
    }

    bool pure(int id) const { return N[id].t == This || N[id].t == Super || N[id].t == Lit; }

    void logical(int id, int depth)
    {
        const Node &n = N[id];
        const QString op = n.str.left(2);
        int target = n.a;
        while (N[target].t == Paren) target = N[target].a;
        const Node &t = N[target];
        if (target != n.a && t.t != Ident) throw ParseError { n.start };   // (o.p) ??= v: not worth it
        const int opLen = 3;
        if (t.t == Ident) {
            R.before(n.start, QStringLiteral("("), depth);
            R.replace(n.x0, opLen, QStringLiteral("%1 (%2 =").arg(op, t.str));
            R.after(N[n.b].end, QStringLiteral("))"), depth);
            return;
        }
        if (t.t != Member || (t.flags & F_OPTIONAL)) throw ParseError { n.start };
        QString obj = text(t.a);
        if (!pure(t.a)) {
            obj = temp("t");
            R.before(n.start, QStringLiteral("((%1 = ").arg(obj), depth);
            R.after(N[t.a].end, QStringLiteral(")"), depth + 1);
        } else {
            R.before(n.start, QStringLiteral("("), depth);
        }
        QString access;
        if (t.flags & F_COMPUTED) {
            if (N[t.b].t == Lit) {
                access = QLatin1Char('[') + text(t.b) + QLatin1Char(']');
            } else {
                const QString k = temp("k");
                R.replace(t.x1, 1, QStringLiteral("[%1 = __qtm.key(").arg(k));
                R.replace(t.x2, 1, QStringLiteral(")]"));
                access = QLatin1Char('[') + k + QLatin1Char(']');
            }
        } else {
            access = QLatin1Char('.') + N[t.b].str;
        }
        R.replace(n.x0, opLen, QStringLiteral("%1 (%2%3 =").arg(op, obj, access));
        R.after(N[n.b].end, QStringLiteral("))"), depth);
    }

    void walk(int id, int depth)
    {
        const Node &n = N[id];
        if (n.t == Func) {
            enter(id, depth);
            for (int k : kids(N, id)) walk(k, depth + 1);
            owners.pop_back();
            return;
        }
        if (n.t == Assign && (n.str == QLatin1String("?\?=") || n.str == QLatin1String("||=") || n.str == QLatin1String("&&=")))
            logical(id, depth);
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- pass 7: async functions

// async function f(a) { body }  ->  function f(a) { return __qtm.async(this, arguments, function* () { body }); }
// await x  ->  (yield x). Async generators run on __qtm.asyncGenerator, where await is
// (yield __qtm.AWAIT(x)) and a plain yield is a value. for await loops step the async iterator.
struct AsyncPass : Pass {
    using Pass::Pass;
    struct Frame {
        int fn;
        bool async, gen, arrow;
        QString superGetter;   // async methods using super: a getter made before entering the generator
    };
    QVector<Frame> frames;

    void run() { walk(P.root, 0); }

    const Frame *asyncFrame() const
    {
        // The function an await belongs to: the innermost one.
        return frames.isEmpty() ? nullptr : &frames.last();
    }
    bool hasLexicalArguments() const
    {
        for (int i = frames.size() - 1; i >= 0; i--)
            if (!frames[i].arrow) return true;
        return false;
    }
    bool isMethod(int fn) const
    {
        const int p = parent[fn];
        return p >= 0 && (N[p].t == Method || (N[p].t == Prop && (N[p].flags & F_METHOD)));
    }
    bool usesSuper(int id) const
    {
        const Node &n = N[id];
        if (n.t == Super) return true;
        if (n.t == Class || (n.t == Func && !(n.flags & F_ARROW))) return false;
        for (int k : kids(N, id))
            if (usesSuper(k)) return true;
        return false;
    }

    void function(int id, int depth)
    {
        const Node &f = N[id];
        Frame fr { id, bool(f.flags & F_ASYNC), bool(f.flags & F_GEN), bool(f.flags & F_ARROW), QString() };
        if (fr.async) {
            const QString helper = fr.gen ? QStringLiteral("__qtm.asyncGenerator") : QStringLiteral("__qtm.async");
            R.replace(f.x0, 5, QString());   // async
            if (fr.gen) R.replace(f.d, 1, QString());   // the generator is the inner function
            const int body = f.b;
            if (fr.arrow) {
                const QString args = hasLexicalArguments() ? QStringLiteral("arguments") : QStringLiteral("null");
                if (f.flags & F_CONCISE) {
                    R.before(N[body].start, QStringLiteral("%1(this, %2, function* () { return (").arg(helper, args), depth);
                    R.after(N[body].end, QStringLiteral("); })"), depth);
                } else {
                    R.replace(N[body].start, 1, QStringLiteral("%1(this, %2, function* () {").arg(helper, args));
                    R.after(N[body].end, QStringLiteral(")"), depth);
                }
            } else {
                QString pre;
                if (isMethod(id) && usesSuper(body)) {
                    fr.superGetter = M.name("sg");
                    pre = QStringLiteral(" const %1 = (k) => super[k];").arg(fr.superGetter);
                }
                R.after(N[body].start + 1, QStringLiteral("%1 return %2(this, arguments, function* () {").arg(pre, helper), depth);
                R.after(N[body].end, QStringLiteral("); }"), depth);
            }
        }
        // An arrow inherits the super getter of the async method it is in.
        if (fr.arrow && !frames.isEmpty() && fr.superGetter.isEmpty()) fr.superGetter = frames.last().superGetter;
        if (!fr.arrow && !fr.async) fr.superGetter.clear();
        frames << fr;
        for (int k : kids(N, id)) walk(k, depth + 1);
        frames.pop_back();
    }

    QString awaitText(const QString &e) const
    {
        const Frame *fr = asyncFrame();
        return fr && fr->gen ? QStringLiteral("(yield __qtm.AWAIT(%1))").arg(e) : QStringLiteral("(yield %1)").arg(e);
    }

    // The innermost async function's super getter, when this super access is inside one.
    QString superGetter() const
    {
        for (int i = frames.size() - 1; i >= 0; i--) {
            if (!frames[i].superGetter.isEmpty()) return frames[i].superGetter;
            if (!frames[i].arrow) return QString();
        }
        return QString();
    }

    void walk(int id, int depth)
    {
        const Node &n = N[id];
        switch (n.t) {
        case Func: function(id, depth); return;
        case Await: {
            const Frame *fr = asyncFrame();
            if (!fr || !fr->async) throw ParseError { n.start };
            R.replace(n.start, 5, fr->gen ? QStringLiteral("(yield __qtm.AWAIT(") : QStringLiteral("(yield"));
            R.after(N[n.a].end, fr->gen ? QStringLiteral("))") : QStringLiteral(")"), depth);
            break;
        }
        case Yield:
            if ((n.flags & F_DELEGATE) && !frames.isEmpty() && frames.last().async)
                throw Unsupported { QStringLiteral("yield* inside an async generator is not supported by this app's JavaScript engine (line %1)")
                                        .arg(lineOf(src, n.start)) };
            break;
        case ForOf:
            if (n.flags & F_AWAIT) {
                const Frame *fr = asyncFrame();
                if (!fr || !fr->async) throw ParseError { n.start };
                const QString it = M.name("i"), res = M.name("r");
                const int left = n.a;
                const QString head = N[left].t == VarDecl ? QStringLiteral("%1 = %2.value;").arg(text(left), res)
                                                          : QStringLiteral("(%1 = %2.value);").arg(text(left), res);
                R.replace(n.x0, n.d - n.x0, QStringLiteral("(let %1 = __qtm.asyncIterator(").arg(it));
                R.replace(n.x2, 1, QStringLiteral("), %1; !(%1 = %2).done; )").arg(res, awaitText(it + QStringLiteral(".next()"))));
                R.before(N[n.c].start, QStringLiteral("{ %1 ").arg(head), depth);
                R.after(N[n.c].end, QStringLiteral(" }"), depth);
                walk(n.b, depth + 1);
                walk(n.c, depth + 1);
                return;
            }
            break;
        case Member:
            if (N[n.a].t == Super) {
                const QString sg = superGetter();
                if (!sg.isEmpty()) {
                    const int p = parent[id];
                    const bool callee = p >= 0 && N[p].t == Call && N[p].a == id;
                    const QString close = callee ? QStringLiteral(").call(this%1").arg(N[p].list.isEmpty() ? QString() : QStringLiteral(", "))
                                                 : QStringLiteral(")");
                    const int end = callee ? N[p].x0 + 1 : n.end;
                    if (n.flags & F_COMPUTED) {
                        R.replace(n.start, N[n.b].start - n.start, sg + QLatin1Char('('));
                        R.replace(N[n.b].end, end - N[n.b].end, close);
                        walk(n.b, depth + 1);
                    } else {
                        R.replace(n.start, end - n.start, QStringLiteral("%1(%2%3").arg(sg, quote(N[n.b].str), close));
                    }
                    if (callee)
                        for (int arg : N[p].list) walk(arg, depth + 1);
                    return;
                }
            }
            break;
        case Call:
            if (N[n.a].t == Member && N[N[n.a].a].t == Super && !superGetter().isEmpty()) {
                walk(n.a, depth + 1);   // rewrites the call's head and walks the arguments
                return;
            }
            break;
        default: break;
        }
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- pass 8: assignments V4 drops

// V4 compiles `return (x = f())` (strict code, so every module) as a tail call of f and drops the
// store; the same for a concise arrow body `(v) => (o.p = f(v))`, and through parentheses, commas,
// ?: and &&, ||, ??. Such an assignment becomes an argument, __qtm.v(x = f()), which V4 compiles
// right.
struct TailPass : Pass {
    using Pass::Pass;

    void run() { walk(P.root, 0); }

    bool endsInCall(int id) const
    {
        const Node &n = N[id];
        switch (n.t) {
        case Call: return true;
        case Paren: return endsInCall(n.a);
        case Seq: return endsInCall(n.list.last());
        case Cond: return endsInCall(n.b) || endsInCall(n.c);
        case Assign: return n.str == QLatin1String("=") && endsInCall(n.b);
        default: return false;
        }
    }
    void tail(int id, int depth)
    {
        const Node &n = N[id];
        switch (n.t) {
        case Paren: tail(n.a, depth + 1); break;
        case Seq: tail(n.list.last(), depth + 1); break;
        case Cond:
            tail(n.b, depth + 1);
            tail(n.c, depth + 1);
            break;
        case Binary:
            if (n.str == QLatin1String("&&") || n.str == QLatin1String("||") || n.str == QLatin1String("?\?")) tail(n.b, depth + 1);
            break;
        case Assign:
            if (n.str == QLatin1String("=") && endsInCall(n.b)) {
                R.before(n.start, QStringLiteral("__qtm.v("), depth);
                R.after(n.end, QStringLiteral(")"), depth);
            }
            break;
        default: break;
        }
    }
    void walk(int id, int depth)
    {
        const Node &n = N[id];
        if (n.t == Return && n.a >= 0) tail(n.a, depth + 1);
        if (n.t == Func && (n.flags & F_CONCISE)) tail(n.b, depth + 1);
        for (int k : kids(N, id)) walk(k, depth + 1);
    }
};

// ---------------------------------------------------------------- driver

template <typename P>
bool runPass(QString &code, Module &m, bool repeat)
{
    for (int round = 0; round < 16; round++) {
        Parser parser(code);
        parser.run();
        P pass(parser, m);
        pass.run();
        if (pass.R.empty()) return true;
        bool ok = false;
        const QString out = pass.R.apply(ok);
        if (!ok) return false;
        code = out;
        if (!repeat) return true;
    }
    return true;
}

} // namespace

Transformed transformModule(const QString &source, const QString &url)
{
    Transformed r;
    r.code = source;
    Module m;
    m.url = url;
    QString code = source;
    try {
        Parser parser(code);
        parser.run();
        r.parsed = true;
        r.imports = parser.imports;
        CheckPass check(parser, m);
        check.run();
        if (!check.R.empty()) {
            bool ok = false;
            code = check.R.apply(ok);
            if (!ok) return r;
        }
    } catch (const ParseError &e) {
        r.errorOffset = e.pos;
        return r;
    } catch (const Unsupported &u) {
        r.parsed = true;
        r.unsupported = u.message;
        return r;
    }
    // The rewrites only produce code the parser reads back; a failure here is a bug in a rewrite,
    // and the original goes to the engine as it is.
    try {
        if (!runPass<ClassPass>(code, m, false) || !runPass<LowerPass>(code, m, true) || !runPass<RestPass>(code, m, true)
            || !runPass<ProtoPass>(code, m, false) || !runPass<SpreadPass>(code, m, false) || !runPass<LogicalPass>(code, m, false) || !runPass<AsyncPass>(code, m, false)
            || !runPass<TailPass>(code, m, false))
            return r;
    } catch (const ParseError &) {
        return r;
    } catch (const Unsupported &u) {
        r.unsupported = u.message;
        return r;
    }
    r.code = code;
    r.changed = code != source;
    return r;
}

QString syntaxErrorAt(const QString &source, int offset)
{
    Lexer lx(source);
    Tok t;
    try {
        t = lx.lex(offset);
    } catch (const ParseError &) {
        return QStringLiteral("Invalid or unexpected token");
    }
    const QString text = source.mid(t.start, t.end - t.start);
    static const QStringList keywords { "break",  "case",   "catch",  "class",    "const",      "continue", "debugger", "default",
                                        "delete", "do",     "else",   "export",   "extends",    "false",    "finally",  "for",
                                        "function", "if",   "import", "in",       "instanceof", "new",      "null",     "return",
                                        "super",  "switch", "this",   "throw",    "true",       "try",      "typeof",   "var",
                                        "void",   "while",  "with",   "enum",     "await" };
    static const QStringList strictReserved { "let", "static", "yield", "implements", "interface", "package", "private", "protected", "public" };
    switch (t.t) {
    case Tk::Eof: return QStringLiteral("Unexpected end of input");
    case Tk::Name:
        if (keywords.contains(text)) return QStringLiteral("Unexpected token '%1'").arg(text);
        if (strictReserved.contains(text)) return QStringLiteral("Unexpected strict mode reserved word");
        return QStringLiteral("Unexpected identifier '%1'").arg(text);
    case Tk::Private: return QStringLiteral("Unexpected identifier '%1'").arg(text);
    case Tk::Num:
    case Tk::BigInt: return QStringLiteral("Unexpected number");
    case Tk::Str: return QStringLiteral("Unexpected string");
    case Tk::Template: return QStringLiteral("Unexpected template string");
    default: return QStringLiteral("Unexpected token '%1'").arg(text);
    }
}

} // namespace qtm::js
