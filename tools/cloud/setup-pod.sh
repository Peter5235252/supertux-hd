#!/usr/bin/env bash
# Setup script for a cloud GPU pod (RunPod / Vast.ai PyTorch template).
# Run once on the pod:  bash setup-pod.sh
# Installs PyTorch (CUDA), Real-ESRGAN, and downloads upscale weights.
set -euo pipefail

python3 -m pip install --upgrade pip
# CUDA 12.x wheels work on RTX 3090/4090/L40S/A100/H100 pods:
python3 -m pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121
python3 -m pip install realesrgan pillow numpy opencv-python-headless

mkdir -p weights
cd weights
# Official Real-ESRGAN weights (canonical release URLs):
curl -fL -o RealESRGAN_x4plus.pth \
  https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth
curl -fL -o RealESRGAN_x4plus_anime_6B.pth \
  https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.3/RealESRGAN_x4plus_anime_6B.pth
# Community 4x-UltraSharp (sharper than x4plus for game textures). Falls back
# to x4plus automatically if this mirror is unreachable:
curl -fL -o 4x-UltraSharp.pth \
  https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x-UltraSharp.pth \
  || echo "WARN: UltraSharp mirror failed, x4plus will be used instead"
cd ..

nvidia-smi | head -12
python3 -c "import torch; print('torch', torch.__version__, 'cuda:', torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else '')"
echo "SETUP OK"
