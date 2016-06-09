mptcpnetlink
============

This repository http://github.com/teto/mptcpnetlink hosts 2 components that allow to control from userspace the MPTCP kernel (http://multipath-tcp.org) path management system:

- The first component is the path management kernel module that will translate MPTCP events into netlink events (so far only one event is supported)
- Then I provide a python daemon which listen to MPTCP netlink events and can tell the kernel module how to react to those events (i.e, if I receive a new remote address, shall I open a new subflow towards this remote address ?)

NB: Netlink is the name of the protocol used by the kernel to let userspace communicate with kernelspace (you also can use syscalls /proc but netlink is more versatile).

This code was used in "Crosslayer Cooperation to Boost Multipath TCP Performance in Cloud Networks":

* [Preprint](https://www-phare.lip6.fr/~secci/papers/CoSePuRaGa-CLOUDNET13.pdf)
* [IEEExplore](http://ieeexplore.ieee.org/xpl/login.jsp?tp=&arnumber=6710558&url=http%3A%2F%2Fieeexplore.ieee.org%2Fiel7%2F6693291%2F6710549%2F06710558.pdf%3Farnumber%3D6710558)




### Current state

As of mptcp v0.88, you can load kernel modules that can react on the following callbacks:

```
static struct mptcp_pm_ops full_mesh __read_mostly = {
        .new_session = full_mesh_new_session,
        .release_sock = full_mesh_release_sock,
        .fully_established = full_mesh_create_subflows,
        .new_remote_address = full_mesh_create_subflows,
        .get_local_id = full_mesh_get_local_id,
        .addr_signal = full_mesh_addr_signal,
        .name = "netlink",
        .owner = THIS_MODULE,
};
```

The current module only propagates new MPTCP sessions events (__new_session__ callback) but it should be pretty easy to extend this to other events.

### REQUIREMENTS

- a 0.88 mptcp kernel (feel free to send pull requests if you managed to make it
  work with a newer release)
- for the python daemon, you need this custom version of libnl (userspace netlink library) that improves the python bindings: https://github.com/teto/libnl_old 


### COMPILATION
To compile the kernel, I refer you to the official documentation: 
http://multipath-tcp.org/pmwiki.php/Users/DoItYourself

If you need a basic _.config_ file, you can use mine: http://github.com/teto/xp_couplage/x86/.config . Otherwise keep in mind there are binary packages available for debian/ubuntu on http://multipath-tcp.org

1. For that you need to patch the official kernel in order to be able to choose source ports.
`mptcp # git apply patches/mptcp88.diff`

2. Compilation of the module kernel:
Move to the "module" folder. If you want to compile against the running kernel just type make, otherwise you need to specify the path to the kernel via: 

```module# KDIR=/lib/modules/3.11.3mptcpbymatt+/build make```

(TODO might miss a make install)
The python daemon does not require any compilation but it relies on a custom version of libnl (hopefully some day I will find the time & the maintener willing to upstream those patches) that you need to compile manually.

3. Compilation of libnl:

```
$ git clone https://github.com/teto/libnl_old libnl && cd libnl
libnl$ ./autogen.sh
libnl$ ./configure
libnl$ make 
libnl$ make install
libnl$ cd python
python# python3 setup.py build
python# python3 setup.py install --user # will install just for your current
user
```
Optional: In case you modify the python code without modifying the bindings, you can also add the libnl/python/build/linux to the PYTHONPATH



### HOW TO USE (once compiled/installed) ?

1. First register the kernel module:
`sysctl -w net.mptcp.mptcp_path_manager="netlink"`

2. Load the python module with the number of subflows to create:
`daemon/daemon.py --simulate 2`
The "--simulate" flag is used to bypass the default system which is described in
our paper (lig retrieves the number of LISP paths)


###More details on how it works
When the kernel detects a new MPTCP connection(=session), it will call the __full\_mesh\_new\_session__ function implemented in your kernel module. My implementation of __full\_mesh\_new\_session__ just multicasts the information (i.e. "hey ! here is a new MPTCP connection") via Netlink to the MPTCP generic netlink family.

Meanwhile, launch a  userspace daemon (python here) that listens to every netlink message sent to the MPTCP netlink  family. When that daemon recieves a __new_session__ notification for instance, it will decide how many subflows to create and then send the answer via netlink back to the same kernel module. The module then does its magic to create the appropriate number of subflows. The module is still basic yet but should be easy to extend.

# TODO

- [ ] Update to latest kernels
- [ ] Upstream libnl patches
- [ ] Support more MPTCP events
- [ ] display a daemon help via -h (with argparse)


