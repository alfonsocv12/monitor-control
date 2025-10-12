# Monitor Control Linux

Mimic of monitor control for macos but for linux Gnome wayland

sudo modprobe i2c-dev
sudo apt install libddcutil-dev ddcutil
i2c-dev
echo "i2c-dev" | sudo tee /etc/modules-load.d/i2c-dev.conf
