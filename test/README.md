testcase
---------

```
sudo cp mirrorlist /etc/nginx/html/
sudo mkdir -p /etc/nginx/html/4.0/x86_64
```

```
isoft-update-server-tool -demo demo1
参考 update.ini.tpl 编辑 demo1/update.ini
cp prescript demo1/update/
cp postscript demo1/update/
拷贝，例如 firefox-40.0-12.x86_64.rpm 到 demo1/update/packages/ 下
isoft-update-server-tool -create demo1 update.xsd
isoft-update-server-tool -check demo1 update.xsd
mkdir upts
cp dem
sudo cp  /usr/share/nginx/html/4.0/x86_64/
```

```
sudo cp updates.xml.xz* /usr/share/nginx/html/4.0/
```
