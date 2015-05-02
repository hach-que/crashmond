# crashmond

A daemon which handles core dumps for Linux applications and submits them as a HTTP PUT request to a server.  You'll need your own HTTP server in place to accept the PUT requests (or you can use https://httpbin.org/put for testing as shown in the commands below).

## Build it

You need clang installed to build the code.

```bash
mkdir build
cd build
cmake ..
make
```

## Run it

```bash
./crashmond /var/run/crashmond.sock https://httpbin.org/put
```

## Install it

```bash
su
mkdir /opt/crashmond
cp crashmond /opt/crashmond/crashmond
cat >/etc/systemd/system/crashmond.service <<EOF
[Unit]
Description=Crash monitoring and reporting service

[Service]
ExecStart=/opt/crashmond/crashmond /var/run/crashmond.sock https://httpbin.org/put
ExecStop=/usr/bin/kill \$MAINPID
Restart=always

[Install]
WantedBy=multi-user.target
EOF
systemctl enable crashmond.service
systemctl start crashmond.service
```
