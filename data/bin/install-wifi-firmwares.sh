#!/bin/sh
#
# Copyright (c) 2010-2012 Haiku, Inc.
# Distributed under the terms of the MIT License.
#
# Authors:
#		Matt Madia, mattmadia@gmail.com
#
# Synopsis:
#	Provide a mechanism for end-users to install various firmwares for wireless
#	network cards in a manner that complies with their individual licenses.
#
# Supported chipsets:
# 	Intel ipw2100
#	Intel ipw2200/2225/2915
#	Broadcom 43xx
#	Marvell 88W8335


MESSAGE="This script will install firmware for various wireless network cards.
 The Broadcom 43xx and Marvell 88W8335 require an active network connection
 to download additional files before installation. In the absence of internet
 access, only Intel's ipw2100 and ipw2200 will be installed.

 If you do not have internet access and need to install the other firmwares,
 goto http://www.haiku-os.org/guides/dailytasks/wireless. This page has
 instructions on which files to manually download and where to copy them into
 this OS. It also has different script that can be run on another OS and will
 prepare a zip archive for easy install. After that, re-run this script."
VIEW='View licenses'
ABORT='Abort installation'
OK='I agree to the licenses. Install firmwares.'

baseURL='http://www.haiku-files.org/files/wifi-firmwares'
firmwareDir=`finddir B_SYSTEM_DATA_DIRECTORY`/firmware
tempDir=`finddir B_COMMON_TEMP_DIRECTORY`/wifi-firmwares
driversDir=`finddir B_SYSTEM_ADDONS_DIRECTORY`/kernel/drivers
intelLicense='/boot/system/data/licenses/Intel (2xxx firmware)'


function DisplayAlert()
{
	local result=`alert --stop "$MESSAGE" "$VIEW" "$ABORT" "$OK"`
	case "${result}" in
		"$VIEW")
			ViewLicenses ;
			DisplayAlert ;
			;;
		"$ABORT")
			exit 0 ;;
		"$OK")
			InstallAllFirmwares ;
			exit 0 ;
			;;
	esac
}


function ViewLicenses()
{
	license="$tempDir/Wifi_Firmware_Licenses"
	cat << EOF > $license

+-----------------------------------------------------------------------------+
|                                                                             |
|   Copyright and licensing information of the various wireless firmwares     |
|                                                                             |
| Firmware for broadcom43xx is under the Copyright of Broadcom Corporation(R) |
| Firmware for marvell88w8335 is under the Copyright of Marvell Technology(R) |
| ipw2100,iprowifi2200 firmware is covered by the following Intel(R) license: |
|                                                                             |
+-----------------------------------------------------------------------------+

EOF
	cat "$intelLicense" >> $license

	open $license
}


function InstallAllFirmwares()
{
	InstallIpw2100
	InstallIprowifi2200
	InstallBroadcom43xx
	InstallMarvell88w8335
}


function UnlinkDriver()
{
	# remove the driver's symlink
	rm -f "${driversDir}/dev/net/${driver}"
}


function SymlinkDriver()
{
	# restore the driver's symlink
	cd "${driversDir}/dev/net/"
	ln -sf "../../bin/${driver}" "${driver}"
}


function DownloadFileIfNotCached()
{
	# DownloadFileIfNotCached <url> <filename> <destination dir>
	local url=$1
	local file=$2
	local dir=$3

	mkdir -p "$dir"
	if [ ! -e $dir/$file ] ; then
		echo "Downloading $url ..."
		wget -nv -O $dir/$file $url
	fi
	result=$?
	if [ $result -gt 0 ]; then
		local error="Failed to download $url."
		local msg="As a result, ${driver}'s firmware will not be installed."
		alert --warning "$error $msg"
	fi
}


function SetFirmwarePermissions()
{
	cd ${firmwareDir}/${driver}/
	for file in * ; do
		if [ "$file" != "$driver" ] && [ -f "$file" ] ; then
			chmod a=r $file
		fi
	done
}


function CleanTemporaryFiles()
{
	rm -rf "$tempDir"
	mkdir -p "$tempDir"
}


function PreFirmwareInstallation()
{
	echo "Installing firmware for ${driver} ..."
	mkdir -p "${firmwareDir}/${driver}"
	UnlinkDriver
}


function PostFirmwareInstallation()
{
	SetFirmwarePermissions
	SymlinkDriver
	CleanTemporaryFiles
	echo "... firmware for ${driver} has been installed."
}


function InstallIpw2100()
{
	driver='iprowifi2100'
	PreFirmwareInstallation

	# Extract contents.
	local file='ipw2100-fw-1.3.tgz'
	local url="${baseURL}/intel/${file}"
	local dir="${firmwareDir}/${driver}"
	DownloadFileIfNotCached $url $file $dir

	# Install the firmware & license file by extracting in place.
	cd "${firmwareDir}/${driver}"
	gunzip < "$file" | tar xf -

	PostFirmwareInstallation
}


function InstallIprowifi2200()
{
	driver='iprowifi2200'
	PreFirmwareInstallation

	# Extract contents.
	local file='ipw2200-fw-3.1.tgz'
	local url="${baseURL}/intel/${file}"
	local dir="${firmwareDir}/${driver}"
	DownloadFileIfNotCached $url $file $dir

	cd "$tempDir"
	gunzip < "${firmwareDir}/${driver}/$file" | tar xf -

	# Install the firmware & license file.
	cd "${tempDir}/ipw2200-fw-3.1"
	mv LICENSE.ipw2200-fw "${firmwareDir}/${driver}/"
	mv ipw2200-ibss.fw "${firmwareDir}/${driver}/"
	mv ipw2200-sniffer.fw "${firmwareDir}/${driver}/"
	mv ipw2200-bss.fw "${firmwareDir}/${driver}/"

	PostFirmwareInstallation
}


function InstallBroadcom43xx()
{
	driver='broadcom43xx'
	PreFirmwareInstallation

	BuildBroadcomFWCutter
	returnCode=$?
	if [ $returnCode -gt 0 ] ; then
		echo "...failed. ${driver}'s firmware will not be installed."
		return $returnCode
	fi

	CutAndInstallBroadcomFirmware
	returnCode=$?
	if [ $returnCode -gt 0 ] ; then
		echo "...failed. ${driver}'s firmware will not be installed."
		return $returnCode
	fi

	PostFirmwareInstallation
}


function InstallMarvell88w8335()
{
	driver='marvell88w8335'
	PreFirmwareInstallation

	# Download firmware archive.
	local file="malo-firmware-1.4.tgz"
	local url="${baseURL}/marvell/${file}"
	local dir="${firmwareDir}/${driver}"
	DownloadFileIfNotCached $url $file "$dir"
	if [ $result -gt 0 ]; then
		echo "...failed. ${driver}'s firmware will not be installed."
		return $result
	fi

	# Extract archive.
	cd "$tempDir"
	tar xf "${firmwareDir}/${driver}/$file"

	# Move firmware files to destination.
	local sourceDir="${tempDir}/share/examples/malo-firmware"
	mv ${sourceDir}/malo8335-h "${firmwareDir}/${driver}"
	mv ${sourceDir}/malo8335-m "${firmwareDir}/${driver}"
	PostFirmwareInstallation
}


function BuildBroadcomFWCutter()
{
	# Download & extract b43-fwcutter.
	local file="b43-fwcutter-012.tar.bz2"
	local dir="${firmwareDir}/${driver}/b43-fwcutter"
	local url="${baseURL}/b43/fwcutter/${file}"
	DownloadFileIfNotCached $url $file $dir
	if [ $result -gt 0 ]; then
		return $result
	fi

	# Extract archive.
	cd "$tempDir"
	tar xjf "$dir/$file"

	# Download additonal files for building b43-fwcutter.
	cd b43-fwcutter-012
	local baseURL='http://cgit.haiku-os.org/haiku/plain/src/system/libroot/posix/glibc'
	DownloadFileIfNotCached ${baseURL}/string/byteswap.h byteswap.h $dir
	if [ $result -gt 0 ]; then
		return $result
	fi
	DownloadFileIfNotCached ${baseURL}/include/arch/x86/bits/byteswap.h byteswap.h $dir/bits
	if [ $result -gt 0 ]; then
		return $result
	fi

	# Copy those files to working directory.
	mkdir -p bits
	cp $dir/byteswap.h .
	cp $dir/bits/byteswap.h bits/

	# Build b43-fwcutter.
	echo "Compiling b43-fwcutter for installing Broadcom's firmware ..."
	make PREFIX=/boot/common CFLAGS="-I. -Wall -D_BSD_SOURCE" > /dev/null 2>&1
	result=$?
	if [ $result -gt 0 ]; then
		echo "... failed to compile b43-fwcutter."
	else
		echo "... successfully compiled b43-fwcutter."
	fi
	if [ ! -e b43-fwcutter ] ; then
		return 1
	fi
	mv b43-fwcutter "$tempDir"

	return 0
}


function CutAndInstallBroadcomFirmware()
{
	# Download firmware.
	local file="wl_apsta-3.130.20.0.o"
	local dir="${firmwareDir}/${driver}"
	local url="${baseURL}/b43/${file}"
	DownloadFileIfNotCached $url $file $dir
	if [ $result -gt 0 ]; then
		return $result
	fi

	# Cut firmware in pieces.
	cp "$dir/$file" "$tempDir"
	cd "$tempDir"
	b43-fwcutter $file > /dev/null 2>&1

	# Rename the pieces.
	cd b43legacy
	for i in $(ls -1); do
		newFileName=$(echo $i | sed "s/\(.*\)\.fw$/bwi_v3_\1/g")
		mv $i $newFileName
	done
	touch bwi_v3_ucode

	# Install files.
	mv * ${firmwareDir}/${driver}/

	return 0
}


mkdir -p "$tempDir"
DisplayAlert
