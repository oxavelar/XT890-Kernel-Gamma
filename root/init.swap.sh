#!/system/bin/sh

/system/xbin/echo 134217728 >/sys/block/zram0/disksize
/system/xbin/echo 134217728 >/sys/block/zram1/disksize

/system/bin/mkswap /dev/block/zram0
/system/bin/mkswap /dev/block/zram1

/system/bin/swapon /dev/block/zram0
/system/bin/swapon /dev/block/zram1
