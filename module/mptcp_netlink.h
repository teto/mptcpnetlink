#ifndef MPTCP_NETLINK_MODULE_H
#define MPTCP_NETLINK_MODULE_H

//trace_ 
// __FILE__ ": "
#define lig_debug(fmt, args...)                   \
    do {                                \
            printk(  fmt, ##args ); \
    } while (0);


/* attributes (variables): the index in this enum is used as a reference for the type,
 *  userspace application has to indicate the corresponding type
 *  the policy is used for security considerations
 */
typedef enum {
    //DOC_EXMPL_A_MSG,
	ELA_RLOCS_NUMBER = 1,	/* number of rlocs u8 */
	ELA_MPTCP_TOKEN,	/* to be able to retrieve correct socket */
	ELA_EID,		/*	= an IP. Only v4 supported as an u32 */
	ELA_MAX

} E_LIG_ATTRIBUTE;

//#define DOC_EXMPL_A_MAX (__DOC_EXMPL_A_MAX - 1)

/* protocol version */
#define LIG_GENL_VERSION 1
#define LIG_GROUP_NAME "lig_daemons"


/*
 * TIPC specific header used in NETLINK_GENERIC requests.
 */
struct lig_genl_msghdr {
	__u32 dest;		/* Destination address */
	__u16 cmd;		/* Command */
	__u16 reserved;		/* Unused */
};

// #define LIG_GENL_HDRLEN	NLMSG_ALIGN(sizeof(struct lig_genl_msghdr))
#define LIG_GENL_HDRLEN	0

/*the name of this family, used by userspace application */
#define LIG_FAMILY_NAME "LIG_FAMILY"



/* commands: enumeration of all commands (functions),
 * used by userspace application to identify command to be ececuted
 */
 typedef enum  {

	/**
	When mptcp asks userspace for the number of rlocs responsible for this EID.
	sends:
	-an EID (only v4 for now => NLA_U32)

	TODO: implement batching mode, asks for several EIDS in a same request
	**/
	ELC_REQUEST_RLOCS_FOR_EID,

	/**
	When userspace returns results, waiting for:
	-the number of rlocs (NLA_U8)
	**/
	ELC_RESULTS,

	/** send eid to use as map resolver **/
	ELC_SET_MAP_RESOLVER,

	/* Facility */
	ELC_MAX
} E_LIG_COMMAND;

// typedef enum E_LIG_COMMAND lig_command_t;

// static char* commandsStr[ELC_MAX + 1] =
// {
// 	[ELC_REQUEST_RLOCS_FOR_EID] = "ELC_REQUEST_RLOCS_FOR_EID",
// 	[ELC_RESULTS]	= "ELC_RESULTS",
// 	[ELC_SET_MAP_RESOLVER] = "ELC_SET_MAP_RESOLVER",
// 	[ELC_MAX]	= "ELC_MAX"
// };


// static char* attributesStr[ELA_MAX + 1] =
// {
// 	[ELA_RLOCS_NUMBER] = "ELA_RLOCS_NUMBER",
// 	[ELA_MPTCP_TOKEN] = "ELA_MPTCP_TOKEN",
// 	[ELA_EID]	= "ELA_EID",
// 	[ELA_MAX]	= "ELA_MAX"
// };

#define NIPQUAD(addr) \
      ((unsigned char *)&addr)[0], \
         ((unsigned char *)&addr)[1], \
         ((unsigned char *)&addr)[2], \
         ((unsigned char *)&addr)[3]



/* attribute policy: defines which attribute has which type (e.g int, char * etc)
 * possible values defined in net/netlink.h
 netlink attribute / action  ?
 */

extern struct nla_policy lig_policies[ ELA_MAX ];
extern struct genl_multicast_group lig_multicast_group;


/* family definition */
extern struct genl_family lig_gnl_family ;


struct ndiffports_priv {
    /* Worker struct for subflow establishment */
    struct work_struct subflow_work;
    struct work_struct netlink_work;

    struct mptcp_cb *mpcb;
};

void send_request_for_eid(struct work_struct *work);

void ndiffports_create_subflows(struct sock *meta_sk);

int handle_results(struct sk_buff *skb_2, struct genl_info *info);

#endif
