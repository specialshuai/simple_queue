使用操作
1.make

2.insmod simple_queue.ko 

3.lsmod

4.mknod /dev/simple_queue c 200 0

5.echo 1 >/dev/simple_queue
  echo 2 >/dev/simple_queue
  echo 3 >/dev/simple_queue
  echo 4 >/dev/simple_queue
  cat /dev/simple_queue
  
6.dmesg -c 查看printk信息
