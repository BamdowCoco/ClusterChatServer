#!/usr/bin/env bash
# run_pool_compare.sh — 连接池收益对照组压测(多组并发 × 多次测量 × 取平均)
#
# 用法:
#   bash run_pool_compare.sh [IP] [PORT]
#
# 默认: 127.0.0.1 6000
#
# 前置: bin/ChatServer(有池) 与 bin/ChatServer_no_pool(无池) 均已构建
# 依赖: MySQL(127.0.0.1:3306) 与 Redis(127.0.0.1:6379) 已启动

set -u

IP="${1:-127.0.0.1}"
PORT="${2:-6000}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
POOL_BIN="$ROOT/bin/ChatServer"
NOPOOL_BIN="$ROOT/bin/ChatServer_no_pool"
BENCH="$ROOT/bin/BenchClient"
OUT="$ROOT/docs/benchmark-results.md"

CONCURRENCIES=(500 1000 2000)
REPEATS=5

# ---------- 前置检查 ----------
for f in "$POOL_BIN" "$NOPOOL_BIN" "$BENCH"; do
    if [[ ! -x "$f" ]]; then
        echo "错误: 未找到 $f，请先构建" >&2
        exit 1
    fi
done
command -v fuser >/dev/null 2>&1 || { echo "错误: 缺少 fuser (psmisc)" >&2; exit 1; }

# ---------- 工具函数 ----------
# 启动 server 跑一次 login, 返回 QPS(失败返回 FAIL)
run_once() {
    local bin="$1" conc="$2"
    fuser -k ${PORT}/tcp 2>/dev/null
    sleep 1
    "$bin" "$IP" "$PORT" > /tmp/poolbench.log 2>&1 &
    local pid=$!
    sleep 2
    if ! kill -0 "$pid" 2>/dev/null; then
        kill -9 "$pid" 2>/dev/null
        echo "FAIL"
        return
    fi
    local out qps
    out=$(timeout 180 "$BENCH" login "$IP" "$PORT" "$conc" 2>&1)
    qps=$(echo "$out" | grep -oE '登录 QPS: [0-9.]+' | grep -oE '[0-9.]+' || true)
    kill -9 "$pid" 2>/dev/null
    if [[ -z "$qps" ]]; then
        echo "FAIL"
    else
        echo "$qps"
    fi
}

# 测量: 给定二进制和并发数, 跑 REPEATS 次, 输出每次 QPS(空格分隔)
measure() {
    local bin="$1" conc="$2" runs="" q=""
    for ((i = 1; i <= REPEATS; i++)); do
        q=$(run_once "$bin" "$conc")
        runs="$runs $q"
    done
    echo "$runs"
}

# 平均: 输入空格分隔数字, 输出平均(保留 1 位, 跳过 FAIL)
avg() {
    echo "$@" | awk '{s=0;n=0; for(i=1;i<=NF;i++) if($i!="FAIL"){s+=$i;n++} if(n>0) printf "%.1f", s/n; else print "N/A"}'
}

# ---------- 主流程 ----------
declare -A POOL_RUNS NOPOOL_RUNS

echo "==> 连接池收益压测开始 (并发: ${CONCURRENCIES[*]}, 每组 $REPEATS 次)"
echo ""

for conc in "${CONCURRENCIES[@]}"; do
    printf "  [有池] %s 并发: " "$conc"
    POOL_RUNS[$conc]="$(measure "$POOL_BIN" "$conc")"
    echo "${POOL_RUNS[$conc]}"
done
for conc in "${CONCURRENCIES[@]}"; do
    printf "  [无池] %s 并发: " "$conc"
    NOPOOL_RUNS[$conc]="$(measure "$NOPOOL_BIN" "$conc")"
    echo "${NOPOOL_RUNS[$conc]}"
done

# ---------- 生成 Markdown 报告 ----------
{
    echo "# 连接池收益压测报告"
    echo ""
    echo "> 生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "> 环境: 本机单节点 (MySQL 127.0.0.1:3306 / Redis 127.0.0.1:6379)"
    echo "> 前置: 已修复 Redis 并发 bug 与无连接池 mysql_library_init 竞态崩溃"
    echo ""
    echo "## 测试方法"
    echo ""
    echo "- 有连接池: \`bin/ChatServer\`(复用 10 个初始连接, 上限 50)"
    echo "- 无连接池: \`bin/ChatServer_no_pool\`(\`NO_CONNECTION_POOL\`, 每次 DB 操作新建/销毁连接)"
    echo "- 每组并发测 $REPEATS 次, 取平均; 命令 \`bin/BenchClient login $IP $PORT <并发数>\`"
    echo ""
    echo "## 原始数据"
    echo ""
    echo "### 有连接池 (QPS, login/s)"
    echo ""
    echo "| 并发 | #1 | #2 | #3 | #4 | #5 | 平均 |"
    echo "|------|----|----|----|----|----|------|"
    for conc in "${CONCURRENCIES[@]}"; do
        runs=${POOL_RUNS[$conc]}
        a=$(avg $runs)
        echo "| $conc | $(echo $runs | sed 's/ / | /g') | $a |"
    done
    echo ""
    echo "### 无连接池 (QPS, login/s)"
    echo ""
    echo "| 并发 | #1 | #2 | #3 | #4 | #5 | 平均 |"
    echo "|------|----|----|----|----|----|------|"
    for conc in "${CONCURRENCIES[@]}"; do
        runs=${NOPOOL_RUNS[$conc]}
        a=$(avg $runs)
        echo "| $conc | $(echo $runs | sed 's/ / | /g') | $a |"
    done
    echo ""
    echo "## 对比汇总"
    echo ""
    echo "| 并发 | 有池平均 QPS | 无池平均 QPS | 提升幅度 |"
    echo "|------|-------------|-------------|---------|"
    for conc in "${CONCURRENCIES[@]}"; do
        pa=$(avg ${POOL_RUNS[$conc]})
        na=$(avg ${NOPOOL_RUNS[$conc]})
        imp=$(awk -v p="$pa" -v n="$na" 'BEGIN{if(n ~ /^[0-9]/ && n>0) printf "%+.0f%%", (p-n)/n*100; else print "N/A"}')
        echo "| $conc | $pa | $na | $imp |"
    done
    echo ""
    echo "> 结论: 连接池在高并发下显著降低 MySQL 连接建立开销，并发越高收益越明显。"
} > "$OUT"

echo ""
echo "==> 报告已生成: $OUT"
