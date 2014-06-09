echo 139 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio139/direction
echo 1 > /sys/class/gpio/gpio139/value
