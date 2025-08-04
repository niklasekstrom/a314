#!/usr/bin/env bash

if [ `id -u` != 0 ] ; then
	echo "Please run flash-cpld.sh using sudo (sudo ./flash-cpld.sh)"
	exit 1
fi

flash_cp() {
	if ! which openFPGALoader > /dev/null 2>&1; then
		echo "Error: openFPGALoader is not installed"
		exit 1
	fi

	RELEASE_INFO=$(curl -s https://api.github.com/repos/niklasekstrom/clockport_pi_interface/releases/latest)
	JED_URL=$(echo "$RELEASE_INFO" | grep -o '"browser_download_url": "[^"]*cp_pi_if\.jed"' | cut -d'"' -f4)
	
	if [ -z "$JED_URL" ]; then
		echo "Error: Could not find cp_pi_if.jed in latest release"
		exit 1
	fi

  echo "Fetching firmware"
	
	TEMP_JED="/tmp/cp_pi_if.jed"
	curl -L -o "$TEMP_JED" "$JED_URL"
	
	if [ ! -f "$TEMP_JED" ]; then
		echo "Error: Failed to download CPLD firmware"
		exit 1
	fi

  echo "Flashing CPLD"
	
	sudo openFPGALoader -c libgpiod --pins 10:8:11:9 -f "$TEMP_JED"
  if [ $? -eq 0 ]; then
      echo "CPLD flashed successfully."
  else
      echo "CPLD flashing failed or was interrupted."
  fi	

	rm -f "$TEMP_JED"
}

flash_cp
