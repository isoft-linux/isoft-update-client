isoft-update-client
--------------------
iSOFT System (Offline) Update Client.

## Build && Install

```
rm daemon/update-generated.*
rm console/update-generated.*
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr    \
    -DLIB_INSTALL_DIR=lib   \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON   \
    -DENABLE_DEBUG=ON   \
    -DENABLE_TEST=ON
make
sudo make install
```

## Daemon

```
sudo systemctl enable isoft-update-daemon.service
sudo systemctl restart isoft-update-daemon.service

sudo systemctl stop isoft-update-daemon.service
cd build
sudo ./daemon/isoft-update-daemon --debug
```
