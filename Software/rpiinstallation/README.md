# validate_pi

A script that tries to validate the Pi install described 
[here](https://github.com/niklasekstrom/a314/wiki/Installation-instructions).



## Prerequisites
: apt-get install jq



## Usage
: ./validate_pi



## Result
If everything seems fine, the script will print `a314 looks fine from here.` to stderr and exit with status 0.

Otherwise you will see an error message and get an exit other than 0.



## TODO
The paths in /etc/opt/a314/a314fs.conf should be checked for owner. I
don't feel like hardcoding `pi` here though, so until we find an OK
heuristic OR the uid is included as a field in
/etc/opt/a314/a314fs.conf, we can let it slide.



Per Weijnitz // hedgewizard
