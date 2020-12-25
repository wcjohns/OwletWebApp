# OwletWebApp
Enhanced alarming and archiving for the Owlet Baby Monitor




# Setting up systemd on Raspberry Pi

```bash
sudo cp owlet-data.service /lib/systemd/system/
sudo cp owlet-web-app.service /lib/systemd/system/

sudo chown root:root /lib/systemd/system/owlet-data.service
sudo chown root:root /lib/systemd/system/owlet-web-app.service

sudo chmod 644 /lib/systemd/system/owlet-data.service
sudo chmod 644 /lib/systemd/system/owlet-web-app.service

sudo systemctl daemon-reload
sudo systemctl enable owlet-data
sudo systemctl enable owlet-web-app
sudo systemctl start owlet-data.service
sudo systemctl start owlet-web-app.service

systemctl status owlet-data

sudo reboot
...
ps -A | grep -i owl
```
