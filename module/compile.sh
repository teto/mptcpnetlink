#!/bin/bash
echo "USAGE: $0 [load|install|compile] as many times as you want "

KDIR="/home/teto/mptcp_src"
moduleName="mptcp_nl"
moduleFolder="/home/teto/xp_couplage/module_v3"
cmd="$1"


for cmd in $@; do

case "$cmd" in
     "remove") 
        echo "remove module"
        rmmod "$moduleName" 2>&1
	;;

    "compile") echo "compiling module"
        make
        ;;
    

    "load") 
            # grep returns 0 if sthg found
            ret=$(grep -c ^${moduleName} /proc/modules)
            echo "Ret $ret"
            if [ $ret -ge 1 ]; then
                echo "Unloading existing module"
                sudo rmmod "$moduleName"
            fi
            echo "Compiling"
            make
            echo "Loading module "
            sudo insmod "${moduleFolder}/${moduleName}.ko"
        ;;

    # set mptcp path manager to this module
    "set")
        sudo sysctl -w net.mptcp.mptcp_path_manager="netlink"
        ;;

     "install") echo "Installing module"
        sudo make install
	;;
esac;

done;
