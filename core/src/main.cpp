#include <iostream>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <dirent.h>
#include <vector>
#include <string>
#include <ddcutil_c_api.h>

class BrightnessMonitor {
private:
    std::vector<int> fds;
    std::vector<DDCA_Display_Handle> display_handles;
    const int BRIGHTNESS_STEP = 10; // Adjust brightness by 10% each time
    const uint8_t VCP_BRIGHTNESS = 0x10; // VCP code for brightness
    
    std::vector<std::string> findKeyboardDevices() {
        std::vector<std::string> devices;
        DIR* dir = opendir("/dev/input");
        if (!dir) {
            std::cerr << "Cannot open /dev/input" << std::endl;
            return devices;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strncmp(entry->d_name, "event", 5) == 0) {
                std::string path = "/dev/input/" + std::string(entry->d_name);
                int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    char name[256] = "Unknown";
                    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                    
                    unsigned long evbit = 0;
                    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);
                    
                    if (evbit & (1 << EV_KEY)) {
                        devices.push_back(path);
                        std::cout << "Found input device: " << name << " (" << path << ")" << std::endl;
                    }
                    close(fd);
                }
            }
        }
        closedir(dir);
        return devices;
    }
    
    bool initDDCUtil() {
        // Initialize libddcutil
        DDCA_Status rc = ddca_init(nullptr, DDCA_SYSLOG_NOT_SET, DDCA_INIT_OPTIONS_NONE);
        if (rc != 0) {
            std::cerr << "ddca_init() failed: " << ddca_rc_name(rc) << std::endl;
            return false;
        }
        
        // Get display information
        DDCA_Display_Info_List* dlist = nullptr;
        rc = ddca_get_display_info_list2(false, &dlist);
        if (rc != 0) {
            std::cerr << "ddca_get_display_info_list2() failed: " << ddca_rc_name(rc) << std::endl;
            return false;
        }
        
        if (dlist->ct == 0) {
            std::cerr << "No DDC capable displays found" << std::endl;
            ddca_free_display_info_list(dlist);
            return false;
        }
        
        std::cout << "Found " << dlist->ct << " DDC capable display(s)" << std::endl;
        
        // Open all displays
        for (int i = 0; i < dlist->ct; i++) {
            DDCA_Display_Ref dref = dlist->info[i].dref;
            DDCA_Display_Handle dh = nullptr;
            
            std::cout << "Opening display " << (i + 1) << ": " << dlist->info[i].model_name << std::endl;
            
            rc = ddca_open_display2(dref, false, &dh);
            if (rc != 0) {
                std::cerr << "  Failed to open display: " << ddca_rc_name(rc) << std::endl;
                continue;
            }
            
            display_handles.push_back(dh);
            std::cout << "  Successfully opened" << std::endl;
        }
        
        ddca_free_display_info_list(dlist);
        
        if (display_handles.empty()) {
            std::cerr << "Failed to open any displays" << std::endl;
            return false;
        }
        
        std::cout << "Successfully initialized DDC connection to " << display_handles.size() << " display(s)" << std::endl;
        return true;
    }
    
    int getCurrentBrightness() {
        if (display_handles.empty()) return -1;
        
        // Get brightness from first display (for display purposes)
        DDCA_Non_Table_Vcp_Value valrec;
        DDCA_Status rc = ddca_get_non_table_vcp_value(display_handles[0], VCP_BRIGHTNESS, &valrec);
        
        if (rc != 0) {
            std::cerr << "Failed to get brightness: " << ddca_rc_name(rc) << std::endl;
            return -1;
        }
        
        return valrec.sh << 8 | valrec.sl; // Current value
    }
    
    bool setBrightness(int value) {
        if (display_handles.empty()) return false;
        
        // Clamp value between 0 and 100
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        
        bool success = true;
        
        // Set brightness on all displays
        for (size_t i = 0; i < display_handles.size(); i++) {
            DDCA_Status rc = ddca_set_non_table_vcp_value(display_handles[i], VCP_BRIGHTNESS, 0, value);
            
            if (rc != 0) {
                std::cerr << "Failed to set brightness on display " << (i + 1) << ": " << ddca_rc_name(rc) << std::endl;
                success = false;
            }
        }
        
        return success;
    }
    
public:
    bool initialize() {
        // Initialize input devices
        auto devices = findKeyboardDevices();
        if (devices.empty()) {
            std::cerr << "No input devices found" << std::endl;
            return false;
        }
        
        for (const auto& device : devices) {
            int fd = open(device.c_str(), O_RDONLY);
            if (fd < 0) {
                std::cerr << "Cannot open " << device << ": " << strerror(errno) << std::endl;
                std::cerr << "Try running with sudo or add user to 'input' group" << std::endl;
                continue;
            }
            fds.push_back(fd);
        }
        
        if (fds.empty()) {
            return false;
        }
        
        // Initialize DDCUtil
        if (!initDDCUtil()) {
            std::cerr << "Warning: DDC initialization failed. Brightness control will not work." << std::endl;
            std::cerr << "Make sure you have i2c permissions (user in i2c group)" << std::endl;
        }
        
        return true;
    }
    
    void onBrightnessUp() {
        std::cout << "🔆 Brightness UP key pressed!" << std::endl;
        
        if (!display_handles.empty()) {
            int current = getCurrentBrightness();
            if (current >= 0) {
                int new_brightness = current + BRIGHTNESS_STEP;
                if (setBrightness(new_brightness)) {
                    std::cout << "   All displays: " << current << "% → " 
                             << (new_brightness > 100 ? 100 : new_brightness) << "%" << std::endl;
                }
            }
        }
    }
    
    void onBrightnessDown() {
        std::cout << "🔅 Brightness DOWN key pressed!" << std::endl;
        
        if (!display_handles.empty()) {
            int current = getCurrentBrightness();
            if (current >= 0) {
                int new_brightness = current - BRIGHTNESS_STEP;
                if (setBrightness(new_brightness)) {
                    std::cout << "   All displays: " << current << "% → " 
                             << (new_brightness < 0 ? 0 : new_brightness) << "%" << std::endl;
                }
            }
        }
    }
    
    void monitor() {
        std::cout << "Monitoring brightness keys... Press Ctrl+C to stop" << std::endl;
        
        if (!display_handles.empty()) {
            int current = getCurrentBrightness();
            if (current >= 0) {
                std::cout << "Current brightness: " << current << "%" << std::endl;
            }
        }
        
        fd_set readfds;
        struct input_event ev;
        
        while (true) {
            FD_ZERO(&readfds);
            int max_fd = 0;
            
            for (int fd : fds) {
                FD_SET(fd, &readfds);
                if (fd > max_fd) max_fd = fd;
            }
            
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            
            int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
            if (ret < 0) {
                std::cerr << "Select error: " << strerror(errno) << std::endl;
                break;
            }
            
            for (int fd : fds) {
                if (FD_ISSET(fd, &readfds)) {
                    ssize_t n = read(fd, &ev, sizeof(ev));
                    if (n == sizeof(ev)) {
                        if (ev.type == EV_KEY && ev.value == 1) {
                            if (ev.code == KEY_BRIGHTNESSUP) {
                                onBrightnessUp();
                            } else if (ev.code == KEY_BRIGHTNESSDOWN) {
                                onBrightnessDown();
                            }
                        }
                    }
                }
            }
        }
    }
    
    ~BrightnessMonitor() {
        for (int fd : fds) {
            close(fd);
        }
        for (DDCA_Display_Handle dh : display_handles) {
            ddca_close_display(dh);
        }
    }
};

int main() {
    BrightnessMonitor monitor;
    
    if (!monitor.initialize()) {
        std::cerr << "Failed to initialize brightness monitor" << std::endl;
        return 1;
    }
    
    monitor.monitor();
    return 0;
}
