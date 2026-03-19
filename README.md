# Monitor Control Linux

Mimic of monitor control for macos but for linux Gnome wayland

```
sudo apt install libddcutil-dev ddcutil libi2c-dev
sudo modprobe i2c-dev
echo "i2c-dev" | sudo tee /etc/modules-load.d/i2c-dev.conf
```
