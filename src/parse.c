#include "9cc.h"

// 現在着目しているトークン
Token *token;

// 入力プログラム
char *user_input;

Node *code[100];

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");  // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op) {
    if (token->kind != TK_RESERVED || strlen(op) != token->len ||
        memcmp(token->str, op, token->len)) {
        return false;
    }

    token = token->next;
    return true;
}

// 次のトークンが識別子のときには、トークンを1つ読み進めて
// そのトークンを返す。それ以外の場合はNULLを返す。
Token *consume_ident() {
    if (token->kind != TK_IDENT) {
        return NULL;
    }

    Token *tok = token;
    token = token->next;
    return tok;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op) {
    if (token->kind != TK_RESERVED || strlen(op) != token->len ||
        memcmp(token->str, op, token->len)) {
        error_at(token->str, "'%c'ではありません", op);
    }
    token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
    if (token->kind != TK_NUM) {
        error_at(token->str, "TK_NUMではありません");
    }

    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof() { return token->kind == TK_EOF; }

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool starts_with(char *p, char *q) { return memcmp(p, q, strlen(q)) == 0; }

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        // 空白文字をスキップ
        if (isspace(*p)) {
            p++;
            continue;
        }

        if (starts_with(p, "==") || starts_with(p, "!=") ||
            starts_with(p, "<=") || starts_with(p, ">=")) {
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '<' ||
            *p == '>' || *p == '(' || *p == ')' || *p == '=' || *p == ';') {
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }

        if ('a' <= *p && *p <= 'z') {
            cur = new_token(TK_IDENT, cur, p++, 1);
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error("トークナイズできません");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}

void print_token(const Token *head) {
    Token *current = token;
    while (current->kind != TK_EOF) {
        switch (current->kind) {
            case TK_RESERVED: {
                printf("TK_RESERVED: ");

                char *str = malloc(sizeof(char) * current->len);
                strncpy(str, current->str, current->len);
                printf("%s", str);
                free(str);

            } break;
            case TK_IDENT: {
                printf("TK_TOKEN: ");

                char *str = malloc(sizeof(char) * current->len);
                strncpy(str, current->str, current->len);
                printf("%s", str);
                free(str);
            } break;
            case TK_NUM:
                printf("TK_NUM: ");
                printf("%d", current->val);
                break;
        }

        printf(",\n");

        current = current->next;
    }
}

// 構文解析
Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

void program() {
    int i = 0;
    while (!at_eof()) {
        code[i++] = stmt();
    }
    code[i] = NULL;
}

Node *stmt() {
    Node *node = expr();
    // printf("expect ; in stmt\n");
    expect(";");
    return node;
}

Node *expr() { return assign(); }

Node *assign() {
    Node *node = equality();
    if (consume("=")) {
        node = new_node(ND_ASSIGN, node, assign());
    }
    return node;
}

Node *equality() {
    Node *node = relational();

    for (;;) {
        if (consume("==")) {
            node = new_node(ND_EQ, node, relational());
        } else if (consume("!=")) {
            node = new_node(ND_NE, node, relational());
        } else {
            return node;
        }
    }
}

Node *relational() {
    Node *node = add();

    for (;;) {
        if (consume("<")) {
            node = new_node(ND_LT, node, add());
        } else if (consume("<=")) {
            node = new_node(ND_LE, node, add());
        } else if (consume(">")) {
            node = new_node(ND_LT, add(), node);
        } else if (consume(">=")) {
            node = new_node(ND_LE, add(), node);
        } else {
            return node;
        }
    }
}

Node *add() {
    Node *node = mul();

    for (;;) {
        if (consume("+")) {
            node = new_node(ND_ADD, node, mul());
        } else if (consume("-")) {
            node = new_node(ND_SUB, node, mul());
        } else {
            return node;
        }
    }
}

Node *mul() {
    Node *node = unary();

    for (;;) {
        if (consume("*")) {
            node = new_node(ND_MUL, node, unary());
        } else if (consume("/")) {
            node = new_node(ND_DIV, node, unary());
        } else {
            return node;
        }
    }
}

Node *unary() {
    if (consume("+")) {
        return primary();
    }

    if (consume("-")) {
        return new_node(ND_SUB, new_node_num(0), primary());
    }

    return primary();
}

Node *primary() {
    // 次のトークンが"("なら、"(" expr ")"のはず
    if (consume("(")) {
        Node *node = expr();
        // printf("expect ) in primary\n");
        expect(")");
        return node;
    }

    Token *tok = consume_ident();
    if (tok) {
        // printf("true");
        Node *node = calloc(1, sizeof(Node));
        node->kind = ND_LVAR;
        node->offset = (tok->str[0] - 'a' + 1) * 8;
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}

void print_node(Node *root) {
    switch (root->kind) {
        case ND_ADD:
            printf("(");
            print_node(root->lhs);
            printf("+");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_SUB:
            printf("(");
            print_node(root->lhs);
            printf("-");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_MUL:
            printf("(");
            print_node(root->lhs);
            printf("*");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_DIV:
            printf("(");
            print_node(root->lhs);
            printf("/");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_EQ:
            printf("(");
            print_node(root->lhs);
            printf("==");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_NE:
            printf("(");
            print_node(root->lhs);
            printf("!=");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_LT:
            printf("(");
            print_node(root->lhs);
            printf("<");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_LE:
            printf("(");
            print_node(root->lhs);
            printf("<=");
            print_node(root->rhs);
            printf(")");
            break;

        case ND_ASSIGN:
            printf("(");
            print_node(root->lhs);
            printf("=");
            print_node(root->rhs);
            printf(")");
            break;
        case ND_LVAR:
            printf("LVAR(offset = %d)", root->offset);
            break;
        case ND_NUM:
            printf("%d", root->val);
            break;
    }
}