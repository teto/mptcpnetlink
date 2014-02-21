#!/usr/bin/python3
import sys
import netlink.capi as nl
import netlink.genl.capi as genl
import traceback
import logging
import argparse
import struct
import socket
import subprocess
import signal
import binascii

#
logger = logging.getLogger( __name__ )
logger.setLevel( logging.DEBUG )
# logger= logging
# print ("handlers", logger.handlers )

handler = logging.StreamHandler()
#logging.FileHandler('hello.log')
handler.setLevel(logging.DEBUG)
logger.addHandler( handler )


# PATH_TOWARDS_PROGRAM = "/home/teto/lig/process_lig_output.sh"

ELC_REQUEST_RLOCS_FOR_EID= 0
ELC_RESULTS =1
ELC_SET_MAP_RESOLVER =2
ELC_MAX=3



LIG_GENL_VERSION=1
LIG_GROUP_NAME	="lig_daemons"
LIG_FAMILY_NAME	="LIG_FAMILY"



# typedef enum {
ELA_RLOCS_NUMBER = 1 #	/* number of rlocs u8 */
ELA_MPTCP_TOKEN = 2 # to be able to retrieve correct socket */
ELA_EID=3 #= an IP. Only v4 supported as an u32 */
ELA_MAX=4 #



logger.debug("Starting LIG DAEMON\n");
# print("level" , logging.getLevelName( logger.getEffectiveLevel() ) )


def sigint_handler(signum, frame):
	print( 'Stop pressing the CTRL+C!' )



class LigDaemon:


	def add_membership(self):
		# nl.nl_socket_drop_membership
		pass

	def __init__(self,lig_program, mapresolver=None,simulate=None):
		self.done = 1;
		# by default, should be possible to override
		self.mapresolver 	= mapresolver or "153.16.49.112";
		self.simulate 		= simulate
		self.lig_program 	= lig_program


		# TODO replace by Socket
		# allocates callback
		tx_cb = nl.nl_cb_alloc(nl.NL_CB_DEFAULT)

		#Clone an existing callback handle
		self.rx_cb = nl.nl_cb_clone(tx_cb)

		# allocates sockets
		self.sk = nl.nl_socket_alloc_cb(tx_cb)




		# set callback handers
		# last parameter represents arguments to pass
		logger.info("Setting callback functions")

		# nl.py_nl_cb_err(self.rx_cb, nl.NL_CB_CUSTOM, error_handler, self);


		# nl_cb_set( callback_set, type, kind, function,args )
		nl.py_nl_cb_set(self.rx_cb, nl.NL_CB_FINISH, nl.NL_CB_VERBOSE, finish_handler, self);
		nl.py_nl_cb_set(self.rx_cb, nl.NL_CB_ACK, nl.NL_CB_VERBOSE, ack_handler, self);
		nl.py_nl_cb_set(self.rx_cb, nl.NL_CB_VALID, nl.NL_CB_CUSTOM, msg_handler, self);
		# nl.py_nl_cb_set(self.rx_cb, nl.NL_CB_VALID, nl.NL_CB_CUSTOM, self.handle, None);

		# Notifications do not use sequence numbers, disable sequence number checking.
		#nl.nl_socket_disable_seq_check(self.sk);
		#nl.nl_socket_disable_auto_ack(self.sk);

		# establish connection
		genl.genl_connect(self.sk)
		self.family_id = genl.genl_ctrl_resolve(self.sk, LIG_FAMILY_NAME)

		# register to the multicast group
		# print( dir( sys.modules["netlink.genl.capi"]) )
		# print( dir( sys.modules["netlink.capi"]) )
		logger.info("family %s registered with number %d"%(LIG_FAMILY_NAME, self.family_id));

		self.group_id = genl.genl_ctrl_resolve_grp (self.sk, LIG_FAMILY_NAME, LIG_GROUP_NAME);
		if self.group_id  < 0 :
			# should log it
			logger.error("Could not find group group %s. Is the adequate module loaded ?"%LIG_FAMILY_NAME)
			exit(1)

		logger.info("Group id found: %d" % self.group_id);
		logger.info("Using mapresolver %s"%self.mapresolver)

		if self.simulate:
			logger.info("Simulation mode enabled %d"%self.simulate)
		else:
			logger.info("Real mode enabled")




		ret = nl.nl_socket_add_membership(self.sk, self.group_id);

		if ret == 0:
			logger.info("Registration successful")
		else:
			logger.error("Could not register to group")
			exit(1)
			



	# send answer via netlink 
	# send it into antoehr thread ?
	def send_rlocs_list_for_eid(self, seq_nb, token, nb_of_rlocs):
		logger.info("Sending rlocs nb of '%d' for token %d with seq nb %d"%(nb_of_rlocs,token,seq_nb))
		msg = nl.nlmsg_alloc()

		# returns void*
		genl.genlmsg_put(msg,
						0, # port
						0, # seq nb
						self.family_id, # family_id
						0, # length of user header
						0, # optional flags
						ELC_RESULTS, 	 # cmd
						LIG_GENL_VERSION # version
						)

		nl.nla_put_u32(msg, ELA_RLOCS_NUMBER, nb_of_rlocs );
		nl.nla_put_u32(msg, ELA_MPTCP_TOKEN , token );

		err = nl.nl_send_auto_complete(self.sk, msg);
		if err < 0:
			logger.error("Error while sending answer")
			nl.nlmsg_free(msg)
			return False

		nl.nlmsg_free(msg)
		return True


	def run(self):
		err = 0
		# cbd.done > 0 and not err < 0
		while True:
			# expects handle / cb configuration
			# see nl.c:965
			err = nl.nl_recvmsgs(self.sk, self.rx_cb)
			# err = nl.nl_recvmsgs_default(self.sk)
			if err < 0:
				logger.error( "Error for nl_recvmsgs: %d: %s"% (err, nl.nl_geterror(err)) )
				break;


	def retrieve_number_of_rlocs(self,eid):

		print("retrieve_number_of_rlocs")
		# if in simulation mode, always return the same answer
		if self.simulate:
			logger.info("Simulation mode returning %d for eid %s"%(self.simulate, eid) )
			return self.simulate



		try:
			#number_of_rlocs=$(lig -m $mapresolver $eid 2>&1 | grep -c up)
			#PATH_TOWARDS_PROGRAM
			cmd= self.lig_program + " -m " + self.mapresolver + eid +" 2>&1" + "| grep -c up" 
			# args = [ self.lig_program,"-m", self.mapresolver, eid , "2>&1" ]
			output = subprocess.check_output( cmd , shell=True);
			print( "Result: ", output.decode() )
			return int( output.decode() );
		except  subprocess.CalledProcessError as e:
			logger.error("Could not retrieve the correct number of rlocs. Return code: %d"%e.returncode)
			return -1


	def handle(self, m):

		print("Hello world from ember function");
		logger.debug("Handle Msg from class")

		try:
			nlmsghdr = nl.nlmsg_hdr(m)
			print("nlmsghdr: flags:", nlmsghdr.nlmsg_flags , "seq:", nlmsghdr.nlmsg_seq )

			genlhdr = genl.genlmsg_hdr( nlmsghdr )
			if not genlhdr:
				logger.error("Could not get generic header")
				return nl.NL_STOP

			if genlhdr.cmd == ELC_REQUEST_RLOCS_FOR_EID:
				logger.info("Request RLOC for an EID")

				
				# attrs = None
				print("Message handler got called");
				err, attrs = genl.py_genlmsg_parse(
						nlmsghdr, 
						0, # will be returned as an attribute
		 				ELA_MAX, 
		 				None
		 				)

				if err < 0:
					logger.error("An error happened while parsing attributes")
					return nl.NL_STOP;


				logger.info("Looking for ELA")
				if ELA_EID in attrs:
					print ("hello", attrs[ELA_EID])
					eid 	= nl.nla_get_u32(attrs[ELA_EID]);
					print ("eid", eid)

					print ("token", attrs[ELA_MPTCP_TOKEN])
					token 	= nl.nla_get_u32(attrs[ELA_MPTCP_TOKEN]);
					print("token", token)

					# print("Requested EID ",eid, " for token ",binascii.hexlify( token ))

					# I => unsigned int
					packed_value = struct.pack('I', eid)
					addr = socket.inet_ntoa(packed_value)

					nb = self.retrieve_number_of_rlocs( addr )
					if nb < 0:
						logger.warning("An error happened while retrieveing nb of rlocs")
						return nl.NL_STOP
					else:
						#nlmsghdr.nlmsg_seq + 1
						self.send_rlocs_list_for_eid(  0, token, nb )
						return nl.NL_SKIP

				else:
					logger.error("Missing critical attribute in packet")

			else:
				logger.warning("Unhandled command %d"% genlhdr.cmd)

			# nlmsg_data returns void* so not usable straightaway

			# TODO need to retrieve command
			# print("e", err)

			return nl.NL_SKIP

		except Exception as e:
			(t,v,tb) = sys.exc_info()
			print( "test", v.message,e )
			traceback.print_tb(tb)

			return nl.NL_SKIP




def error_handler(err, a):
	print("error handler")
	logger.error("Error handler called")
	# a.done = err.error
	return nl.NL_STOP

def finish_handler(m, arg):
	print("finish handler")
	logger.info("finish_handler called")
	return nl.NL_SKIP

def ack_handler(m, arg):
	print("ack handler")
	logger.info("ack handler called")
	# arg.done = 0
	return nl.NL_STOP


def msg_handler(m, arg):

	print("msg handler called")
	# print ( dir (arg) )
	arg.handle(m)
	# return nl.NL_OK
	return nl.NL_SKIP 

###############################
###############################
## TO TEST LIBNL (remove later)
###############################
msg_handler = "hello world"
ack_handler = None

if __name__ == '__main__':


	signal.signal(signal.SIGINT, sigint_handler)

	# run tests
	parser = argparse.ArgumentParser(
		description='Daemon listeling for mptcp netlink requests'
		)

	#there must be at most one ?
	parser.add_argument('mapresolver', nargs="?", default="153.16.49.112", #DEFAULT_MAPRESOLVER, 
	                  help="Choose")

	parser.add_argument('--simulate', dest="number_of_subflows", type=int)

	# subparsers    = parser.add_subparsers(dest="mode", help='sub-command help')
	# parser = subparsers.add_parser('daemon',help='daemon help')

	args = parser.parse_args( sys.argv[1:] )

	try:
		# could pass mr, or simulate mode
		#
		daemon = LigDaemon(
					lig_program="/home/teto/lig/lig" ,
					mapresolver=args.mapresolver, 
					simulate=(args.number_of_subflows or None) 
					)


		daemon.run();


	except Exception as e:
		# (type, value, traceback)
		(t, v, tb) = sys.exc_info()
		print("hello world",v )
		traceback.print_tb(tb)

