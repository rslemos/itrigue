echo 143 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio143/direction
echo 1 > /sys/class/gpio/gpio143/value
