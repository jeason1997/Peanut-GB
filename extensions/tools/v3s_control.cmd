@echo off
cd /d "%~dp0"
python v3s_control.py
if errorlevel 1 (
  echo Install dependency if missing: python -m pip install pyserial
  pause
)
