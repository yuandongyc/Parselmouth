# 构建 Praat WASM

本目录介绍如何利用 GitHub Actions 构建 Praat 的 WebAssembly (WASM) 版本。

## 目录结构

```
cpp_wasm_yd/
├── README.md              # 本文档
├── .github/workflows/     # GitHub Actions 工作流配置
└── src/                   # 源代码（可选）
```

## 前置条件

1. **Fork 本仓库** 到你的 GitHub 账号
2. 准备一个用于部署 WASM 文件的平台（如 GitHub Pages、Cloudflare Pages 等）

## GitHub Actions 工作流配置

### 1. 创建工作流文件

在 `.github/workflows/build-wasm.yml` 创建以下内容：

```yaml
name: Build Praat WASM

on:
  push:
    branches: [ master ]
  pull_request:
    branches: [ master ]
  workflow_dispatch:  # 允许手动触发

jobs:
  build-wasm:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        submodules: recursive  # 递归克隆子模块（Praat）
    
    - name: Install Emscripten
      uses: mymindstorm/setup-emsdk@v14
      with:
        version: '3.1.48'  # 或其他稳定版本
        actions-cache-folder: 'emsdk-cache'
    
    - name: Verify Emscripten installation
      run: |
        emcc --version
        em++ --version
    
    - name: Install CMake
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake
    
    - name: Configure CMake for WASM
      run: |
        emcmake cmake -B build-wasm \
          -DCMAKE_BUILD_TYPE=Release \
          -DPARSELMOUTH_INSTALL_DIR=install \
          -DPYTHON_EXECUTABLE=$(which python3) \
          -DBUILD_SHARED_LIBS=OFF
    
    - name: Build WASM
      run: |
        cmake --build build-wasm -- -j$(nproc)
    
    - name: Package WASM artifacts
      run: |
        mkdir -p artifacts
        cp build-wasm/*.wasm artifacts/ 2>/dev/null || true
        cp build-wasm/*.js artifacts/ 2>/dev/null || true
        cp build-wasm/*.data artifacts/ 2>/dev/null || true
        cp -r praat-wasm/*.html artifacts/ 2>/dev/null || true
    
    - name: Upload artifacts
      uses: actions/upload-artifact@v4
      with:
        name: praat-wasm-artifacts
        path: artifacts/
        retention-days: 30
    
    - name: Deploy to GitHub Pages
      if: github.ref == 'refs/heads/master' && github.event_name == 'push'
      uses: peaceiris/actions-gh-pages@v3
      with:
        github_token: ${{ secrets.GITHUB_TOKEN }}
        publish_dir: ./artifacts
```

### 2. 自定义构建配置

根据项目需求，你可能需要调整 CMake 配置参数：

```yaml
- name: Configure CMake for WASM
  run: |
    emcmake cmake -B build-wasm \
      -DCMAKE_BUILD_TYPE=Release \
      -DPARSELMOUTH_BUILD_STATIC=ON \
      -DPARSELMOUTH_INSTALL_DIR=install \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_CXX_FLAGS="-O3 -s WASM=1 -s USE_SDL=2"
```

## 优化构建选项

### 1. 减小 WASM 文件大小

```yaml
- name: Build WASM with optimizations
  run: |
    emcc main.cpp \
      -o praat.html \
      -s WASM=1 \
      -s USE_SDL=2 \
      -O3 \
      -s MIN_CHROME_VERSION=90 \
      -s MIN_FIREFOX_VERSION=90 \
      -s ALLOW_MEMORY_GROWTH=1 \
      --closure 1
```

### 2. 启用多线程

```yaml
- name: Build with threading
  run: |
    emcmake cmake -B build-wasm \
      -DCMAKE_BUILD_TYPE=Release \
      -DPARSELMOUTH_ENABLE_PTHREADS=ON
```

## 部署选项

### 1. GitHub Pages

在 GitHub 仓库设置中：
1. 进入 **Settings** → **Pages**
2. 选择 **Source** 为 **GitHub Actions**
3. 保存后，每次推送都会自动部署

### 2. Cloudflare Pages

在工作流中添加：

```yaml
- name: Deploy to Cloudflare Pages
  uses: cloudflare/wrangler-action@v3
  with:
    apiToken: ${{ secrets.CLOUDFLARE_API_TOKEN }}
    accountId: ${{ secrets.CLOUDFLARE_ACCOUNT_ID }}
    command: pages deploy artifacts --project-name=praat-wasm
```

### 3. 预构建 Docker 环境

如果需要更复杂的环境，可以使用 Docker：

```yaml
- name: Build with Docker
  run: |
    docker run --rm \
      -v ${{ github.workspace }}:/workspace \
      -w /workspace \
      emscripten/emsdk:latest \
      emcc main.cpp -o praat.wasm
```

## 本地构建测试

在推送到 GitHub 前，可以在本地测试构建：

```bash
# 安装 Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# 构建
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm

# 运行本地服务器
python3 -m http.server 8000
# 访问 http://localhost:8000
```

## 常见问题

### 1. 内存不足

```yaml
- name: Build with increased memory
  run: |
    emcmake cmake -B build-wasm \
      -DCMAKE_CXX_FLAGS="-s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=256MB"
```

### 2. 子模块未正确克隆

```yaml
- name: Checkout with submodules
  uses: actions/checkout@v4
  with:
    submodules: recursive
    token: ${{ secrets.GITHUB_TOKEN }}  # 确保有权限访问子模块
```

### 3. 构建时间过长

使用缓存：

```yaml
- name: Cache Emscripten
  uses: actions/cache@v4
  with:
    path: emsdk-cache
    key: ${{ runner.os }}-emsdk-${{ hashFiles('**/emsdk_version') }}

- name: Cache CMake build
  uses: actions/cache@v4
  with:
    path: build-wasm
    key: ${{ runner.os }}-build-wasm-${{ hashFiles('**/CMakeLists.txt') }}
```

## 资源链接

- [Emscripten 官方文档](https://emscripten.org/)
- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [Praat 官方网站](http://www.praat.org/)
- [WASM 优化指南](https://web.dev/fast-webassembly/)

## 许可证

遵循项目根目录的 GPL-3.0-or-later 许可证。
