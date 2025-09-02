# Janus Vulkan Miner — VIPOR (baked-in 1% dev fee)

Built for vipor.net Warthog with Vulkan compute SHA-256t + CPU VerusHash v2.2 (fetched at build).

Defaults:
- Pool: stratum+tcp://usw.vipor.net:5020
- Main wallet: a2c4b7c5cb9efaf6ee4fbf2b66dbea3cb279ab2e54f18e47.testphone
- Dev-fee wallet: 640c98518964d4e94348a09773d7e9a2add9ce34261211cd.devfee (1% time)
- Password: x

Build:
  sudo apt update
  sudo apt install -y build-essential cmake git libvulkan1 vulkan-tools mesa-vulkan-drivers glslang-tools spirv-tools
  ./build.sh

Run:
  cd build
  ./janusv
