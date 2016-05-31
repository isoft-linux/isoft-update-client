## iSOFTLinux System Update Daemon WorkFlow

![iSOFTLinux System Update daemon workflow](https://raw.github.com/isoft-linux/isoft-update-client/master/daemon/flow1.png)

***DISADVANTAGE*** of installing Update packages during libraries and services that are currently running:

* command line reboot or poweroff will break the update workflow;
* hardware off will break it too;
* conflicts.

![iSOFTLinux Offline System Updates with the help of systemd](https://raw.github.com/isoft-linux/isoft-update-client/master/daemon/flow2.png)

***ADVANTAGE*** of systemd offline system update:
* systemd booted into special system-update.target with a few of services started;
* it will goto special target time and time again (without removing the symlink) if break the workflow;
* remove symlink after update, then reboot to normal default.target;
* textmode UI with update percentage.

## Testcase for ArchLinux

```
sudo pacman -S packagekit
```

Change update script to your own
```
cd /usr/lib/systemd/system/system-update.target.wants/
mv packagekit-offline-update.service packagekit-offline-update.service.bak
cp ../packagekit-offline-update.service ../isoft-offline-update.service
```

Edit ExecStart=/usr/lib/isoft-update-client/isoft-offline-update

```
#!/bin/bash 
plymouth display-message --text="iSOFTLinux Offline System Updating..."
sleep 13
rm /system-update
reboot
```
