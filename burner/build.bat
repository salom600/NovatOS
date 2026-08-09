@echo off
REM NovatOS Burner build script (runs on Windows GitHub Actions runner)
REM Compiles main.py into a standalone NovatOSBurner.exe via PyInstaller

echo === NovatOS USB Burner build ===

REM Install Python dependencies
pip install pyinstaller pillow --upgrade

REM Create icon if not present
if not exist novatos.ico (
    echo Creating placeholder icon...
    python -c "from PIL import Image; img = Image.new('RGBA', (256, 256), (15, 17, 23, 255)); img.save('novatos.ico')"
)

REM Build the .exe
pyinstaller --onefile --windowed --name NovatOSBurner --icon=novatos.ico --noconfirm --clean src\main.py

REM Show result
echo.
echo === Build complete ===
dir dist\NovatOSBurner.exe
