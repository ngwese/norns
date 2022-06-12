#!/usr/bin/env bash
cd "$(dirname "$0")"

# only update new disk image
version=$(cat ~/version.txt)
if [ $version -lt 220306 ];
then
	echo "needs new disk image, aborting."
	exit;
fi;

# basic repo updates
sudo rm -rf /home/we/norns
cp -a norns /home/we/
sudo rm -rf /home/we/maiden
cp -a maiden /home/we/

# version/changelog
cp version.txt /home/we/
cp changelog.txt /home/we/

# rewrite journalctl
sudo cp config/journald.conf /etc/systemd/
sudo mkdir -p /var/log/journal

# TODO !!!!!! FIX THIS FOR v3
# rewrite matron.sevice
sudo cp --remove-destination config/norns-matron.service /etc/systemd/system/norns-matron.service

# add watcher
sudo cp --remove-destination config/norns-watcher.service /etc/systemd/system/norns-watcher.service
sudo systemctl enable norns-watcher

# packages
#sudo dpkg -i package/*.deb

# norns-image
#cd /home/we/norns-image
#./setup.sh

# update jack systemd
sudo cp --remove-destination config/norns-jack.service /etc/systemd/system/norns-jack.service

# scrub invisibles
find ~/dust -name .DS_Store -delete
find ~/dust -name ._.DS_Store -delete

# maiden project setup
cd /home/we/maiden
./project-setup.sh

# get common audio if not present
if [ ! -d /home/we/dust/audio/common ]; then
	echo "does not exist, downloading"
	cd /home/we/dust/audio
	wget https://github.com/monome/norns/releases/download/v2.7.1/dust-audio-common.tgz
	tar xzvf dust-audio-common.tgz
	rm dust-audio-common.tgz
fi

# set alsa volume
amixer --device hw:sndrpimonome set Master 100% on
sudo alsactl store

# cleanup
rm -rf ~/update/*
