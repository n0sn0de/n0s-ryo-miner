#!/bin/bash
# Native nosnode validation helper.
# Default mode is benchmark because it is more reproducible than share acceptance.
set -e

REMOTE="nosnode"
REPO_URL="https://github.com/n0sn0de/n0s-ryo-miner.git"
REMOTE_DIR="/home/nosnode/n0s-ryo-miner"
CUDA_HOME="${CUDA_HOME:-/usr/local/cuda}"
CUDA_ARCH="${CUDA_ARCH:-75}"
MODE="${MODE:-benchmark}"
POOL="${POOL:-192.168.50.186:3333}"
TIMEOUT="${TIMEOUT:-45}"

echo "====================================="
echo "nosnode Native CUDA Validation"
echo "Remote: ${REMOTE}"
echo "CUDA home: ${CUDA_HOME}"
echo "Mode: ${MODE}"
echo "====================================="

BRANCH="${BRANCH:-$(git rev-parse --abbrev-ref HEAD)}"
echo "Deploying ${BRANCH} to ${REMOTE}..."
ssh "${REMOTE}" "rm -rf ${REMOTE_DIR} && git clone -b ${BRANCH} ${REPO_URL} ${REMOTE_DIR} 2>&1 | tail -3"

echo "Building on nosnode..."
ssh "${REMOTE}" "set -e &&   export PATH=\$HOME/.local/bin:${CUDA_HOME}/bin:\$PATH &&   export LD_LIBRARY_PATH=${CUDA_HOME}/lib64:\$LD_LIBRARY_PATH &&   cd ${REMOTE_DIR} && rm -rf build && mkdir build && cd build &&   cmake .. -DCUDA_ENABLE=ON -DOpenCL_ENABLE=OFF -DMICROHTTPD_ENABLE=ON     -DN0S_COMPILE=generic -DCUDA_ARCH='${CUDA_ARCH}' -DCMAKE_BUILD_TYPE=Release &&   cmake --build . -j\$(nproc) &&   test -f bin/n0s-ryo-miner"

echo "✅ Native CUDA build succeeded on nosnode"

if [ "${MODE}" = "benchmark" ]; then
  echo "Running benchmark..."
  ssh "${REMOTE}" "set -e &&     export LD_LIBRARY_PATH=${CUDA_HOME}/lib64:\$LD_LIBRARY_PATH &&     cd ${REMOTE_DIR} && timeout 130 ./build/bin/n0s-ryo-miner --noAMD       --benchmark 10 --benchmark-json ${REMOTE_DIR}/benchmark.json >/dev/null 2>&1 || true &&     cat ${REMOTE_DIR}/benchmark.json"
  exit 0
fi

echo "Mining for ${TIMEOUT} seconds..."
OUTPUT=$(ssh "${REMOTE}" "export LD_LIBRARY_PATH=${CUDA_HOME}/lib64:\$LD_LIBRARY_PATH &&   cd ${REMOTE_DIR}/build/bin && timeout ${TIMEOUT} ./n0s-ryo-miner --noAMD     -o ${POOL} -u WALLET -p x 2>&1" || true)

if echo "$OUTPUT" | grep -q "CUDA.*ERROR\|Error.*cuda\|INVALID_DEVICE"; then
  echo "❌ CUDA errors detected"
  echo "$OUTPUT" | grep -iE "error|warning|cuda|failed" | head -10
  exit 1
fi

SHARES=$(echo "$OUTPUT" | grep -c "Share accepted" || true)
if [ "$SHARES" -gt 0 ]; then
  echo "✅ CUDA mining works — ${SHARES} shares accepted"
  echo "$OUTPUT" | grep -E "HASHRATE|Share accepted" | tail -10
  exit 0
else
  echo "⚠️  No shares observed; benchmark mode may be more reliable for smoke tests"
  echo "$OUTPUT" | grep -E "Pool|connected|logged|GPU" | tail -10
  exit 1
fi
