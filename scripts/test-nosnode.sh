#!/bin/bash
# Test CUDA 12.6 build on nosnode (RTX 2070, Turing sm_75)
set -e

REMOTE="nosnode"
POOL="${POOL:-192.168.50.186:3333}"
TIMEOUT="${TIMEOUT:-45}"
REPO_URL="https://github.com/n0sn0de/xmr-stak.git"
REMOTE_DIR="/home/nosnode/xmr-stak-test"

echo "====================================="
echo "nosnode CUDA 12.6 Test"
echo "GPU: RTX 2070 (Turing, sm_75)"
echo "CUDA: 12.6 (driver 535.288.01)"
echo "Pool: $POOL"
echo "====================================="

# Deploy code
echo "Deploying to nosnode..."
BRANCH="${BRANCH:-$(git rev-parse --abbrev-ref HEAD)}"
ssh $REMOTE "rm -rf $REMOTE_DIR && git clone -b $BRANCH $REPO_URL $REMOTE_DIR 2>&1 | tail -3"

# Build (NVIDIA only, no AMD)
echo "Building with CUDA 12.6..."
ssh $REMOTE "set -e && \
  export PATH=\$HOME/.local/bin:/usr/local/cuda-12.6/bin:\$PATH && \
  export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:\$LD_LIBRARY_PATH && \
  cmake --version | head -1 && \
  cd $REMOTE_DIR && rm -rf build && mkdir build && cd build && \
  cmake .. -DCUDA_ENABLE=ON -DOpenCL_ENABLE=OFF -DMICROHTTPD_ENABLE=ON \
    -DCUDA_ARCH='75' -DCMAKE_BUILD_TYPE=Release 2>&1 && \
  cmake --build . -j\$(nproc) 2>&1 | tail -10 && \
  test -f bin/n0s-ryo-miner"

if [ $? -ne 0 ]; then
    echo "❌ BUILD FAILED"
    exit 1
fi
echo "✅ CUDA 12.6 build successful"

# Mine test
echo ""
echo "Mining for ${TIMEOUT} seconds..."
OUTPUT=$(ssh $REMOTE "export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:\$LD_LIBRARY_PATH && \
  cd $REMOTE_DIR/build/bin && timeout $TIMEOUT ./n0s-ryo-miner --noAMD \
  -o $POOL -u WALLET -p x 2>&1" || true)

# Check for errors
if echo "$OUTPUT" | grep -q "CUDA.*ERROR\|Error.*cuda\|INVALID_DEVICE"; then
    echo "❌ CUDA ERRORS DETECTED:"
    echo "$OUTPUT" | grep "CUDA.*ERROR\|Error.*cuda\|INVALID_DEVICE" | head -10
    exit 1
fi

# Check for shares
SHARES=$(echo "$OUTPUT" | grep -c "Share accepted" || true)
if [ "$SHARES" -gt 0 ]; then
    echo "✅ CUDA 12.6 MINING WORKS — $SHARES shares accepted"
    echo "$OUTPUT" | grep "HASHRATE\|Share accepted" | tail -10
    exit 0
else
    echo "⚠️  NO SHARES FOUND (may need longer timeout)"
    echo "$OUTPUT" | grep -E "Pool|connected|logged|GPU" | tail -10
    exit 1
fi
