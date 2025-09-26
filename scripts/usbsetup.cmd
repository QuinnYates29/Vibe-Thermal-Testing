@echo off
usbipd list > usb_output.txt
for /f "tokens=1 USEBACKQ" %%a in (`findstr /r /c:" *J-Link" usb_output.txt`) do (
    echo J-Link on bus %%a
    usbipd bind --busid %%a
    usbipd attach --wsl --busid %%a
)
