#!/usr/bin/env bash
# run_bench.sh — 一键压测集群聊天服务器
#
# 用法:
#   bash run_bench.sh [IP] [PORT] [CONN] [LOGIN] [SENDERS] [MSGS]
#
# 默认: 127.0.0.1 6000 1000 500 50 100
#
# 前置: 先构建出 bin/ChatServer 与 bin/BenchClient (cmake --build build)

set -u

IP="${1:-127.0.0.1}"
PORT="${2:-6000}"
CONN="${3:-1000}"
LOGIN="${4:-500}"
SENDERS="${5:-50}"
MSGS="${6:-100}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER="$ROOT/bin/ChatServer"
BENCH="$ROOT/bin/BenchClient"
LOG_FILE="/tmp/chatserver_bench.log"

# ---------- 前置检查 ----------
if [[ ! -x "$SERVER" ]]; then
    echo "错误: 未找到 $SERVER，请先构建: cmake --build build" >&2
    exit 1
fi
if [[ ! -x "$BENCH" ]]; then
    echo "错误: 未找到 $BENCH，请先构建: cmake --build build" >&2
    exit 1
fi

SERVER_PID=""
TMP_DIR=""

cleanup()
{
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "==> 停止服务端 (pid=$SERVER_PID)"
        kill "$SERVER_PID" 2>/dev/null || true
    fi
    [[ -n "$TMP_DIR" ]] && rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# ---------- 启动服务端 ----------
echo "==> 启动服务端 $IP:$PORT ..."
"$SERVER" "$IP" "$PORT" > "$LOG_FILE" 2>&1 &
SERVER_PID=$!
sleep 2

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "错误: 服务端启动失败，日志见 $LOG_FILE" >&2
    tail -20 "$LOG_FILE" >&2
    exit 1
fi

# ---------- 压测 ----------
TMP_DIR="$(mktemp -d)"
CONN_OUT="$TMP_DIR/conn.txt"
LOGIN_OUT="$TMP_DIR/login.txt"
CHAT_OUT="$TMP_DIR/chat.txt"

echo ""
echo "========== 压测开始 =========="
echo "目标: $IP:$PORT | conn=$CONN login=$LOGIN chat=${SENDERS}x${MSGS}"
echo ""

echo "--- [1/3] 并发连接 ---"
"$BENCH" conn "$IP" "$PORT" "$CONN" | tee "$CONN_OUT"
echo ""

echo "--- [2/3] 登录吞吐 ---"
"$BENCH" login "$IP" "$PORT" "$LOGIN" | tee "$LOGIN_OUT"
echo ""

echo "--- [3/3] 单聊消息吞吐 ---"
"$BENCH" chat "$IP" "$PORT" "$SENDERS" "$MSGS" | tee "$CHAT_OUT"
echo ""

# ---------- 汇总 ----------
extract()
{
    grep -oE "$2" "$1" 2>/dev/null | grep -oE '[0-9]+(\.[0-9]+)?' | head -1
}
CONN_RATE="$(extract "$CONN_OUT" '建立速率: [0-9.]+')"
LOGIN_QPS="$(extract "$LOGIN_OUT" '登录 QPS: [0-9.]+')"
SEND_TPS="$(extract "$CHAT_OUT" '发送 TPS: [0-9.]+')"
RECV_TPS="$(extract "$CHAT_OUT" '接收 TPS: [0-9.]+')"

echo "========== 汇总报告 =========="
echo "| 指标         | 数值 |"
echo "|--------------|------|"
echo "| 连接建立速率 | ${CONN_RATE:-N/A} conn/s |"
echo "| 登录 QPS     | ${LOGIN_QPS:-N/A} login/s |"
echo "| 单聊发送 TPS | ${SEND_TPS:-N/A} msg/s |"
echo "| 单聊接收 TPS | ${RECV_TPS:-N/A} msg/s |"
echo "==============================="
echo "服务端日志: $LOG_FILE"
