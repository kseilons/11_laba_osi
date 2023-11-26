cmd_/home/vboxuser/Desktop/11_lab/xwrand.mod := printf '%s\n'   xwrand.o | awk '!x[$$0]++ { print("/home/vboxuser/Desktop/11_lab/"$$0) }' > /home/vboxuser/Desktop/11_lab/xwrand.mod
