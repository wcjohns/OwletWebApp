# OwletWebApp
Enhanced alarming and data archiving for the [Owlet Baby Monitor](https://owletcare.com).

cmake -DCMAKE_PREFIX_PATH=/prefix/install/path/ ..




# Setting up systemd on Raspberry Pi

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




# Compiling dependancies
On Linux and macOS you should only need to install [boost](https://www.boost.org) and [Wt](https://www.webtoolkit.eu/wt/) 4.x, and [cmake](http://cmake.org).
You can can either install through your package manager, or compiling from source shouldnt be too bad.

To compile boost and Wt from source on macOS, first make sure you have Xcode installed, and its command line utilities, as well as cmake, then run commands similar to:
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

