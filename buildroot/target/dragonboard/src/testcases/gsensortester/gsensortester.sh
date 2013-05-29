#!/bin/sh

source send_cmd_pipe.sh
source script_parser.sh

module_path=`script_fetch "gsensor" "module_path"`
device_name=`script_fetch "gsensor" "device_name"`

echo "insmod $module_path"
insmod "$module_path"
if [ $? -ne 0 ]; then
    SEND_CMD_PIPE_FAIL $3
    exit 1
fi

sleep 3

for event in $(cd /sys/class/input; ls event*); do
    name=`cat /sys/class/input/$event/device/name`
    case $name in
        $device_name)
            SEND_CMD_PIPE_OK $3
            exit 0
            ;;
        *)
            ;;
    esac
done

SEND_CMD_PIPE_FAIL $3
