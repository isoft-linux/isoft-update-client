## iSOFTLinux System Update Daemon WorkFlow

![iSOFTLinux System Update daemon workflow](https://raw.github.com/isoft-linux/isoft-update-client/master/daemon/flow1.png)

***DISADVANTAGE*** of installing Update packages during libraries and services that are currently running:

* command line ```reboot``` or ```poweroff``` will break the update workflow;
* hardware turn off computer will break too;
* conflicts with running services.

Thanks for [systemd Offline System Updates](https://www.freedesktop.org/software/systemd/man/systemd.offline-updates.html), isoft-update-client daemon does not need to use systemd-logind Inhibit, but use powerful system-update.target:

![iSOFTLinux Offline System Update](https://raw.github.com/isoft-linux/isoft-update-client/master/daemon/flow2.png)

***ADVANTAGE*** of systemd offline system update:
* systemd booted into special system-update.target with a few of services started;
* it will goto special target time and time again (without removing the symlink) if break the workflow;
* remove symlink after update, then reboot to normal default.target;
* textmode UI with update percentage.

## Testcase for ArchLinux

Change update script to your own

```
$ mkdir /usr/lib/systemd/system/system-update.target.wants
$ touch /usr/lib/systemd/system/isoft-offline-update.service
```

isoft-offline-update.service
```
[Unit]                                                                             
Description=Updates the operating system whilst offline                            
Requires=dbus.socket                                                               
OnFailure=reboot.target                                                            
                                                                                   
[Service]                                                                          
ExecStart=/usr/lib/isoft-update-client/isoft-offline-update
```

simple isoft-offline-update script:
```
#!/bin/bash
sleep 13
plymouth display-message --text="iSOFTLinux Offline System Updating..."
sleep 13
plymouth display-message --text="Updated successfully, rebooting..."
rm /system-update
reboot
```

PackageKit provides nicely formatted output [offline update](https://github.com/hughsie/PackageKit/blob/master/client/pk-offline-update.c) please fork it for your own system update flow ;-)
