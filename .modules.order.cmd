cmd_/media/shared/AOS/modules.order := {   echo /media/shared/AOS/register_device.ko;   echo /media/shared/AOS/constants.h; :; } | awk '!x[$$0]++' - > /media/shared/AOS/modules.order
