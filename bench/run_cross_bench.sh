#!/usr/bin/env bash
# run_cross_bench.sh — 本机双实例跨节点转发延迟压测
#
# 用法:
#   bash run_cross_bench.sh [IP1] [PORT1] [IP2] [PORT2] [MSGS]
#
# 默认: 127.0.0.1 6000 127.0.0.1 6001 1000
#
# 前置: 先构建出 bin/ChatServer 与 bin/BenchClient (cmake --build build)
# 依赖: Redis(127.0.0.1:6379) 与 MySQL 已启动

set -u

IP1="${1:-127.0.0.1}"
PORT1="${2:-6000}"
IP2="${3:-127.0.0.1}"
PORT2="${4:-6001}"
MSGS="${5:-1000}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER="$ROOT/bin/ChatServer"
BENCH="$ROOT/bin/BenchClient"
LOG1="/tmp/chatserver_cross1.log"
LOG2="/tmp/chatserver_cross2.log"

# ---------- 前置检查 ----------
if [[ ! -x "$SERVER" ]]; then
    echo "错误: 未找到 $SERVER，请先构建: cmake --build build" >&2
    exit 1
fi
if [[ ! -x "$BENCH" ]]; then
    echo "错误: 未找到 $BENCH，请先构建: cmake --build build" >&2
    exit 1
fi

SERVER1_PID=""
SERVER2_PID=""

cleanup()
{
    # 用 SIGINT 触发服务端优雅关闭(reset 用户状态)
    for pid in "$SERVER1_PID" "$SERVER2_PID"; do
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            echo "==> 停止服务端 (pid=$pid)"
            kill -INT "$pid" 2>/dev/null || true
        fi
    done
}
trap cleanup EXIT

# ---------- 启动两个服务端 ----------
echo "==> 启动 server1 $IP1:$PORT1 ..."
"$SERVER" "$IP1" "$PORT1" > "$LOG1" 2>&1 &
SERVER1_PID=$!

echo "==> 启动 server2 $IP2:$PORT2 ..."
"$SERVER" "$IP2" "$PORT2" > "$LOG2" 2>&1 &
SERVER2_PID=$!

sleep 2

for pid in "$SERVER1_PID" "$SERVER2_PID"; do
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "错误: 服务端启动失败，日志见 $LOG1 / $LOG2" >&2
        tail -20 "$LOG1" >&2 2>/dev/null || true
        tail -20 "$LOG2" >&2 2>/dev/null || true
        exit 1
    fi
done

# ---------- 跨节点延迟压测 ----------
echo ""
echo "========== 跨节点转发延迟压测 =========="
echo "server1: $IP1:$PORT1 | server2: $IP2:$PORT2 | 消息数: $MSGS"
echo ""

"$BENCH" cross "$IP1" "$PORT1" "$IP2" "$PORT2" "$MSGS"

echo ""
echo "服务端日志: $LOG1 / $LOG2"
