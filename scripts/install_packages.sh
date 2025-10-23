#!/bin/bash

# update and upgrade
sudo apt update
sudo apt upgrade -y

# install dependancies
sudo apt -y install git
sudo apt -y install make
sudo apt -y install gfortran
sudo apt -y install libgtest-dev

# re-check and cleanup
sudo apt upgrade -y --fix-missing
sudo apt autoremove -y
