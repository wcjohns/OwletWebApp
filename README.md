# OwletWebApp
Enhanced alarming and data archiving for the [Owlet Smart Sock](https://owletcare.com).

This README is a work in progress, and totally not complete.

## Motivation
The Owlet Smart Sock app only alarms when oxygen saturation goes below 80%, or the heartrate goes outside of the 60 to 220 bpm range (see comments from the company on [this page](https://blog.owletcare.com/understanding-pulse-oximetry-and-how-owlet-uses-it/)),
and the history graph only shows 10-minute averages, and only for the most recent 30 days or so.
These thresholds and capabilities are probably okay for most use cases (I assume, they are experts - I'm definetly not), but if your baby has known health conditions, or your risk profile differs from typical, there is no way to adjust thresholds in the app.

[OwletWebApp](http://github.com/wcjohns/OwletWebApp) allows you to specify alarm thresholds, archive an unlimited timespan of data, and view live and previous data of each measurement taken by the device.

This project uses code derived from [CAB426/owletpy](https://github.com/CAB426/owletpy) to grab the data uploaded from the Owlet monitor, every 30 seconds or so.  This data is then put in a sqlite3 database, which is monitored by the by the web-app server created by this project.  This web-app server runs on a computer on your home wifi, so that any device connected to your wifi can access this web-app.


## Screenshots
![iPhone Screenshot](docs/images/screenshot_iphone.png?raw=true "Screenshot on iPhone")

![Settings Screenshot](add docs/images/screenshot_settings.png?raw=true "Some of the settings available")


## Compiling and setting up OwletWebApp
Instructions are a work in progress, and I'll probably eventually provide some pre-compiled binaries.

### Compiling dependancies
On Linux and macOS you should only need to install [cmake](http://cmake.org), [boost](https://www.boost.org), and [Wt](https://www.webtoolkit.eu/wt/) 4.x.
You can can either install through your package manager, or compiling from source shouldnt be too bad.

To compile boost and Wt from source on macOS, first make sure you have Xcode installed, and its command line utilities, as well as cmake, then run commands similar to (Linux is similar process):
```bash
# Set where we want to install the libraries to
export MY_WT_PREFIX=/path/to/install/prefix/macOS_wt4.5.0_prefix

# Download boost, compile, and install
curl https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz --output boost_1_75_0.tar.gz
tar -xzvf boost_1_75_0.tar.gz
cd boost_1_75_0
./b2 toolset=clang link=static variant=release threading=multi cxxflags="-std=c++11 -stdlib=libc++" linkflags="-std=c++11 -stdlib=libc++" --prefix=${MY_WT_PREFIX} -j12 install

# Download Wt, compile, and install
curl https://github.com/emweb/wt/archive/4.5.0.tar.gz --output wt-4.5.0.tar.gz
tar -xzvf wt-4.5.0.tar.gz
cd wt-4.5.0
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBS=OFF -DCONNECTOR_FCGI=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF -DBoost_USE_STATIC_LIBS=ON -DCMAKE_PREFIX_PATH=${MY_WT_PREFIX} -DCMAKE_INSTALL_PREFIX=${MY_WT_PREFIX} -DCONFIGDIR=${MY_WT_PREFIX}/etc/etc -DCONFIGURATION=${MY_WT_PREFIX}/etc/wt/wt_config.xml -DWTHTTP_CONFIGURATION=${MY_WT_PREFIX}/etc/wt/wthttpd -DRUNDIR=${MY_WT_PREFIX}/var/run/wt  ..
make -j12 install
```

### Compiling this code
```bash
cd OwletWebApp
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=/prefix/install/path/ ..
make -j12
```


### Setting up systemd on Raspberry Pi

```bash
useradd -r -s /usr/bin/nologin owlet
usermod -a -G owlet $USER

sudo cp owlet-data.service /lib/systemd/system/
sudo cp owlet-web-app.service /lib/systemd/system/

sudo chown owlet:owlet /lib/systemd/system/owlet-data.service
sudo chown owlet:owlet /lib/systemd/system/owlet-web-app.service

sudo chmod 644 /lib/systemd/system/owlet-data.service
sudo chmod 644 /lib/systemd/system/owlet-web-app.service

sudo systemctl daemon-reload
sudo systemctl enable owlet-data
sudo systemctl enable owlet-web-app
sudo systemctl start owlet-data.service
sudo systemctl start owlet-web-app.service

systemctl status owlet-data

# Or to do some debugging
sudo journalctl -f


sudo reboot
...
ps -A | grep -i owl
```
