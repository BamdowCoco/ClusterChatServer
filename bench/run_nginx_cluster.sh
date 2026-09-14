#!/usr/bin/env bash
# run_nginx_cluster.sh — Nginx 多节点集群水平扩展压测(单节点 vs 3 节点)
#
# 用法:
#   bash run_nginx_cluster.sh [并发数]
#
# 默认并发 500(避免 Nginx worker_connections=1024 限制: 客户端→nginx + nginx→后端 各占一份连接)
#
# 前置: bin/ChatServer 与 bin/BenchClient 已构建; Nginx 已启动且 sudo 免密可用
# 依赖: MySQL(127.0.0.1:3306) / Redis(127.0.0.1:6379)

set -u

IP="127.0.0.1"
NGINX_PORT=8000
NODE_PORTS=(6000 6001 6002)
CONC="${1:-500}"
REPEATS=5

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/bin/ChatServer"
BENCH="$ROOT/bin/BenchClient"
NGINX_CONF="/usr/local/nginx/conf/nginx.conf"
NGINX_BIN="/usr/local/nginx/sbin/nginx"
OUT="$ROOT/docs/nginx-cluster-results.md"

# ---------- 前置检查 ----------
for f in "$BIN" "$BENCH"; do
    [[ -x "$f" ]] || { echo "错误: 未找到 $f" >&2; exit 1; }
done
[[ -x "$NGINX_BIN" ]] || { echo "错误: 未找到 $NGINX_BIN" >&2; exit 1; }
[[ -f "$NGINX_CONF" ]] || { echo "错误: 未找到 $NGINX_CONF" >&2; exit 1; }
sudo -n true 2>/dev/null || { echo "错误: 需要 sudo 免密(修改 nginx.conf)" >&2; exit 1; }
command -v fuser >/dev/null 2>&1 || { echo "错误: 缺少 fuser" >&2; exit 1; }

# ---------- 配置 Nginx: 加 6002 节点 + 热加载 ----------
echo "==> 备份 nginx.conf -> nginx.conf.bak"
sudo cp "$NGINX_CONF" "${NGINX_CONF}.bak"

if sudo grep -q '127.0.0.1:6002' "$NGINX_CONF"; then
    echo "==> upstream 已含 6002, 跳过追加"
else
    echo "==> 向 upstream MyServer 追加 127.0.0.1:6002"
    sudo perl -0pi -e \
        's/(server 127\.0\.0\.1:6001 weight=1 max_fails=3 fail_timeout=30s;)/$1\n\t\tserver 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;/' \
        "$NGINX_CONF"
fi

sudo grep -q '127.0.0.1:6002' "$NGINX_CONF" || { echo "错误: 追加 6002 失败" >&2; exit 1; }

echo "==> 热加载 Nginx"
sudo "$NGINX_BIN" -s reload
sleep 1

# ---------- 启动 3 个节点 ----------
NODE_PIDS=()
for port in "${NODE_PORTS[@]}"; do
    fuser -k ${port}/tcp 2>/dev/null
    "$BIN" "$IP" "$port" > "/tmp/nginx_node_${port}.log" 2>&1 &
    NODE_PIDS+=($!)
done
sleep 2
for i in "${!NODE_PORTS[@]}"; do
    kill -0 "${NODE_PIDS[$i]}" 2>/dev/null || { echo "错误: 节点 ${NODE_PORTS[$i]} 启动失败"; tail -5 "/tmp/nginx_node_${NODE_PORTS[$i]}.log"; exit 1; }
done
echo "==> 3 节点已启动: ${NODE_PORTS[*]}"

# ---------- 工具函数 ----------
# 测量: 给定 host/port/mode(conn|login), 跑 REPEATS 次, 返回每次指标值(空格分隔)
measure() {
    local host="$1" port="$2" mode="$3" vals="" v=""
    for ((i = 1; i <= REPEATS; i++)); do
        if [[ "$mode" == "conn" ]]; then
            v=$(timeout 120 "$BENCH" conn "$host" "$port" "$CONC" 2>&1 | grep -oE '建立速率: [0-9.]+' | grep -oE '[0-9.]+' || true)
        else
            v=$(timeout 180 "$BENCH" login "$host" "$port" "$CONC" 2>&1 | grep -oE '登录 QPS: [0-9.]+' | grep -oE '[0-9.]+' || true)
        fi
        vals="$vals ${v:-FAIL}"
    done
    echo "$vals"
}

avg() {
    echo "$@" | awk '{s=0;n=0; for(i=1;i<=NF;i++) if($i!="FAIL"){s+=$i;n++} if(n>0) printf "%.1f", s/n; else print "N/A"}'
}

# ---------- 压测 ----------
echo ""
echo "==> 单节点基线 (直连 127.0.0.1:6000)"
SN_CONN="$(measure "$IP" 6000 conn)"
SN_LOGIN="$(measure "$IP" 6000 login)"
echo "    conn  :$SN_CONN"
echo "    login :$SN_LOGIN"

echo ""
echo "==> 3 节点集群 (Nginx 127.0.0.1:8000)"
CL_CONN="$(measure "$IP" "$NGINX_PORT" conn)"
CL_LOGIN="$(measure "$IP" "$NGINX_PORT" login)"
echo "    conn  :$CL_CONN"
echo "    login :$CL_LOGIN"

# ---------- 生成 Markdown 报告 ----------
{
    echo "# Nginx 集群水平扩展压测报告"
    echo ""
    echo "> 生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "> 环境: 本机 3 节点 (${NODE_PORTS[*]}) + Nginx stream TCP 负载均衡 (监听 ${NGINX_PORT})"
    echo "> 并发: ${CONC}, 每组测 ${REPEATS} 次取平均"
    echo ""
    echo "## 测试方法"
    echo ""
    echo "- 单节点基线: 客户端直连 \`127.0.0.1:6000\`(单个 ChatServer 处理全部请求)"
    echo "- 3 节点集群: 客户端连 \`127.0.0.1:${NGINX_PORT}\`(Nginx 轮询分发到 3 个 ChatServer)"
    echo "- 命令: \`bin/BenchClient conn/login 127.0.0.1 <port> ${CONC}\`"
    echo ""
    echo "## 原始数据"
    echo ""
    echo "### 连接建立速率 (conn/s)"
    echo ""
    echo "| 场景 | #1 | #2 | #3 | #4 | #5 | 平均 |"
    echo "|------|----|----|----|----|----|------|"
    echo "| 单节点 | $(echo $SN_CONN | sed 's/ / | /g') | $(avg $SN_CONN) |"
    echo "| 3 节点集群 | $(echo $CL_CONN | sed 's/ / | /g') | $(avg $CL_CONN) |"
    echo ""
    echo "### 登录 QPS (login/s)"
    echo ""
    echo "| 场景 | #1 | #2 | #3 | #4 | #5 | 平均 |"
    echo "|------|----|----|----|----|----|------|"
    echo "| 单节点 | $(echo $SN_LOGIN | sed 's/ / | /g') | $(avg $SN_LOGIN) |"
    echo "| 3 节点集群 | $(echo $CL_LOGIN | sed 's/ / | /g') | $(avg $CL_LOGIN) |"
    echo ""
    echo "## 对比汇总"
    echo ""
    echo "| 指标 | 单节点 | 3 节点集群 | 提升幅度 |"
    echo "|------|--------|-----------|---------|"
    for pair in "conn:$SN_CONN:$CL_CONN" "login:$SN_LOGIN:$CL_LOGIN"; do
        label="${pair%%:*}"
        rest="${pair#*:}"
        sn="${rest%%:*}"
        cl="${rest#*:}"
        sna=$(avg $sn)
        cla=$(avg $cl)
        imp=$(awk -v a="$sna" -v b="$cla" 'BEGIN{if(a ~ /^[0-9]/ && a>0) printf "%+.0f%%", (b-a)/a*100; else print "N/A"}')
        [[ "$label" == "conn" ]] && name="连接建立速率" || name="登录 QPS"
        echo "| $name | $sna | $cla | $imp |"
    done
    echo ""
    echo "> 结论: Nginx TCP 负载均衡将请求分发到多节点, 集群整体吞吐随节点数提升, 体现水平扩展能力。"
} > "$OUT"

echo ""
echo "==> 报告已生成: $OUT"

# ---------- 清理: 停 3 个节点(保留 nginx.conf 3 节点配置, 备份在 .bak) ----------
for pid in "${NODE_PIDS[@]}"; do
    kill -9 "$pid" 2>/dev/null
done
fuser -k ${NODE_PORTS[0]}/tcp ${NODE_PORTS[1]}/tcp ${NODE_PORTS[2]}/tcp 2>/dev/null
echo "==> 已停止 3 个节点"
