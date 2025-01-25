#!/bin/bash
#Script to make a .deb package of the executables one directory above
#Edit the "ver" file in this directory to change major version
#Edit the "postinst" to edit the postinstall script

#Define some basic directories
src_dir="$(dirname "$(realpath "$0")")/.."
script_dir="$(dirname "$(realpath "$0")")"

#project definitions, edit for each project
name="kbled"
progtype="utils"
ver=$(cat "$script_dir/ver")
rev=`date +%Y%02m%02d`
arch="amd64"
maintainer="Maintainer: Michael Curtis <chememjc@gmail.com>"
shortdescription="Individually addressable keyboard LED backlight control for IT-829x controllers"
longdescription="Individually addressable keyboard LED backlight control for ITE IT-829x controllers used in System76 Bonobo WS (bonw15) / Clevo X370 laptops"
service="kbled.service"
config="kbled.conf"
pkgconfigfiles=("postinst" "uninst" ".conffiles")

#internal definitions
pkgname="$name""_$ver-$rev""_$arch"
pkgnamedeb="$name""_$ver-$rev""_$arch.deb"
origin_dir="$script_dir/$pkgname"
util_dir="$src_dir/utils"
bin_dir="/usr/bin"
systemd_dir="/etc/systemd/system"
config_dir="/etc"
debian_dir="/DEBIAN"
depends=$(cat "$script_dir/dependencies")
# Define multiple extensions to look for in util_dir
util_extensions=("sh" "py")

if [ $# -eq 1 ] || [ $# -eq 0 ]
then
  if [ "$1" == "clean" ]; then
    str="$script_dir/$name""_*"
    echo "clean up dist tree: rm -rf $str"
    rm -rf $str
    echo "clean up dist .deb file: rm -f $script_dir/*.debr"
    rm -f $script_dir/*.deb
    exit 0
  fi 
else
  echo "Syntax: $0 <package name> <version>"
  exit 1
fi

#Print out the key variables
echo "Src_dir: $src_dir"
echo "script_dir: $src_dir"
echo "Package Name: $pkgname"
echo "Version: $ver"
echo "Revision: $rev"
echo "Base Path: $script_dir"
echo "Utility Path: $util_dir"
echo "Package Full Name: $pkgnamedeb"
echo "Dependencies: $depends"

#Make the directories
mkdir -p "$origin_dir$debian_dir"
mkdir -p "$origin_dir$bin_dir"

#Ensure the destination directory exists
if [ ! -d "$origin_dir" ]; then
  echo "Error: Destination directory [$origin_dir] does not exist."
  exit 1
fi

#Copy all executable files into the staged usr/bin
for file in "$src_dir"/*; do
  # Check if the file is executable
  if [ -x "$file" ] && [ -f "$file" ]; then
    # Copy the executable file to the destination directory
    cp "$file" "$origin_dir/$bin_dir"
    echo "Added: $file to $bin_dir"
  fi
done

for file in "$util_dir"/*; do
  #Check if the file is executable and exists
  if [ -x "$file" ] && [ -f "$file" ]; then
    # Get the filename without the path
    filename="${file##*/}"
    copied=false

    #Check if the filename ends with any of the specified extensions
    for ext in "${util_extensions[@]}"; do
      if [[ "$filename" == *.$ext ]]; then
        # Remove the specified extension if it matches
        cp "$file" "$origin_dir/$bin_dir/${filename%.$ext}"
        echo "Added: $filename to $bin_dir (removed .$ext)"
        copied=true
        break  # Stop checking other extensions if one matches
      fi
    done

    #If no extension matched, check if there's no extension at all
    if [ "$copied" = false ] && [[ "$filename" != *.* ]]; then
      cp "$file" "$origin_dir/$bin_dir/$filename"
      echo "Added: $filename to $bin_dir (no extension)"
      copied=true
    fi

    #If no extension was matched or if there was no extension, copy the file as-is
    if [ "$copied" = false ]; then
      cp "$file" "$origin_dir/$bin_dir/$filename"
      echo "Added: $filename to $bin_dir (kept original name)"
    fi
  fi
done

#Copy the configuration files
if [ service != "" ]; then
    mkdir -p "$origin_dir$systemd_dir"
    cp "$src_dir/$service" "$origin_dir$systemd_dir"
    echo "Copied $service to $systemd_dir"
fi

if [ config != "" ]; then
    mkdir -p "$origin_dir$config_dir"
    cp "$src_dir/$config" "$origin_dir$config"
    echo "Copied $config to $config_dir"
fi

#Add the files to the DEBIAN directory
echo "Source: $name" > "$origin_dir$debian_dir/control"
echo "Package: $name" >> "$origin_dir$debian_dir/control"
echo "Version: $ver-$rev" >> "$origin_dir$debian_dir/control"
echo "Section: $progtype" >> "$origin_dir$debian_dir/control"
echo "Priority: optional" >> "$origin_dir$debian_dir/control"
echo "Architecture: $arch" >> "$origin_dir$debian_dir/control"
echo "Depends: $depends" >> "$origin_dir$debian_dir/control"
echo "Maintainer: $maintainer" >> "$origin_dir$debian_dir/control"
echo "Description: $shortdescription" >> "$origin_dir$debian_dir/control"
echo " $longdescription" >> "$origin_dir$debian_dir/control"

#add the pre/postinstall and uninstall scripts/files
for file in "${pkgconfigfiles[@]}"; do
    echo "Copying $file script/file to $debian_dir"
    cp "$script_dir/$file" "$origin_dir$debian_dir/$file"
    chmod +x "$origin_dir$debian_dir/$file"
done
# don't make the .conffile executable if it is included:
chmod -x "$origin_dir$debian_dir/.conffiles" | true

echo "making $pkgnamedeb package"
dpkg-deb --build --root-owner-group $origin_dir
