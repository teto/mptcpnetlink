

#include <linux/genetlink.h>
#include <linux/inet.h>
#include <net/genetlink.h>
// #include <net/core/utils.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <net/mptcp.h>
#include "mptcp_netlink.h"

// For tests only
#include <linux/hrtimer.h>
#include <linux/netdevice.h>

#define MAX_STRING     16
#define MAX_TAB        16
#define TIMER_DELAY 20000



struct nla_policy lig_policies[ ELA_MAX ] = {
    // [ELA_RLOCS_NUMBER] = { .type = NLA_U8 },
    [ELA_RLOCS_NUMBER] = { .type = NLA_U8 },

    /**
    An EID is an IP, for now only IPv4 is supported.
    **/
    [ELA_EID] = { .type = NLA_U32 },
    /* usefeul to idetify associated socket, know if it's still active  */
    [ELA_MPTCP_TOKEN] = { .type = NLA_U32 }
};

struct genl_multicast_group lig_multicast_group =
{
    .name = LIG_GROUP_NAME
};

struct genl_family lig_gnl_family = {
    .id = GENL_ID_GENERATE,         /* genetlink should generate an id */
    .hdrsize = LIG_GENL_HDRLEN,
    .name = LIG_FAMILY_NAME,        /*the name of this family, used by userspace application */
    .version = LIG_GENL_VERSION,                   //version number
    .maxattr = ELA_MAX    /* a changer? ARRAY_SIZE */
};

/*
commands: mapping between the command enumeration and the actual function
genlmsg_reply(
*/


//u32 eid, u32 token
void send_request_for_eid(struct work_struct *work)
{
    struct ndiffports_priv *pm_priv = container_of(work,
                             struct ndiffports_priv,
                             netlink_work);
    
    struct mptcp_cb *mpcb = pm_priv->mpcb;
    struct sock *meta_sk = mpcb->meta_sk;

    struct sk_buff *skb = 0;
    int rc = 0;
    void *msg_head;
    struct in_addr *daddr = 0;
    u32 token = 0;
    u32 eid = 0;
    lig_debug( "%d", __LINE__);
    mpcb = tcp_sk(meta_sk)->mpcb;
   
    lig_debug( "%d", __LINE__);
    daddr = (struct in_addr *)&inet_sk(meta_sk)->inet_daddr;
    eid = daddr->s_addr;
    token = mpcb->mptcp_loc_token;


lig_debug( "sending request to userspace for eid %u.%u.%u.%u and token %#x \n", NIPQUAD(eid), token );
    // lig_debug("Sending request for ");
    // /* TODO request */
    // if( send_request_for_eid(daddr->s_addr,  mpcb->mptcp_loc_token ) < 0 ){
    //     //
    //     lig_debug("Could not send request for token %#x", mpcb->mptcp_loc_token);
    // }

    /* send a message back
    allocate some memory, since the size is not yet known use NLMSG_GOODSIZE
    GPF kernel take smore time but more chance to get memory
    */
    skb = genlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);

    if (skb == NULL)
    {
        lig_debug( "could not allocate space for new msg\n");
        return;
    }

    msg_head = genlmsg_put(
                        skb,
                        0,            /* pid  */
                        0, /* no de seq (NL_AUTO_SEQ ne marche pas) */
                        &lig_gnl_family,
                        LIG_GENL_HDRLEN,    /* header length (to check) */
                        ELC_REQUEST_RLOCS_FOR_EID   /* command */
                        );

    if (msg_head == NULL) {
        lig_debug( "could not create generic header\n");
        return;
    }

    rc = nla_put_u32( skb, ELA_MPTCP_TOKEN, token );

    if (rc != 0)
    {
        lig_debug( "could not add token \n");
        return;
    }


    rc = nla_put_u32( skb, ELA_EID, eid);
    // rc = nla_put_string(skb, ELA_EID, "hello world from kernel space\n");
    if (rc != 0)
    {
        lig_debug( KERN_ERR "could not add eid \n");
        return;
    }

    /* finalize the message */
    genlmsg_end(skb, msg_head);

    //struct sk_buff *skb, u32 pid,unsigned int group, gfp_t flags)
    rc = genlmsg_multicast(
        skb,
        0,  /* set own pid to not recevie . 0 looks ok might ? */
        lig_multicast_group.id,
        GFP_KERNEL  /* allows sleep. More likely to succed than GFP_ATOMIC */
         );


    if(rc != 0)
    {
        lig_debug( "could not multicast packet to group [%d], error [%d]\n", lig_multicast_group.id, rc);

        /* no such process */
        if (rc == -ESRCH)
        {
            lig_debug( KERN_ERR "Should be because daemon is not running\n");

        }
        // TODO may be error here maybe it is free by genlmsg_multicast ?
        //nlmsg_free(skb);
        return;
    }
    lig_debug( "Successfully multicasted\n");
    // nlmsg_free(skb);
    return;
}




int handle_results(struct sk_buff *skb_2, struct genl_info *info)
{

    struct nlattr *nla = 0;
    struct genlmsghdr* pGenlhdr = info->genlhdr;
    int cmd = 0;
    u32 token = 0;


    cmd = pGenlhdr->cmd;
    lig_debug("Command is: [%d] n", cmd );

    switch(cmd)
    {
        case ELC_RESULTS:
            lig_debug("recieved a priori the number of rlocs for the EID\n");
		    /*
		    for each attribute there is an index in info->attrs which points to
		    a nlattr structure
		    in this structure the data is given
		     */
            nla = info->attrs[ELA_MPTCP_TOKEN];

            if (nla == 0) {
                lig_debug("No MPTCP token available for current host \n");
            }
            else {
                token = nla_get_u32(nla);
                lig_debug("Received nla of type %d and len %d. TOken value: [%#x]  ", nla->nla_type, nla->nla_len, token);
            }


            // Normalement c'est parse par nla_parse
            nla = info->attrs[ELA_RLOCS_NUMBER];


            // lig_debug("Module debug data: %p, %p , %p, %p\n",info->attrs[ELA_MPTCP_TOKEN], info->attrs[ELA_MAX], info->attrs[ELA_EID], info->attrs[ELA_RLOCS_NUMBER] );
            if (nla)
            {
            	/* I need to retrieve that sk from the token */
            	// ndiffports_create_subflows
            	// need to decrease the counter afterwards ! via atomic_decrease
            	struct sock* meta_sk;
            	struct mptcp_cb *mpcb;
            	u32 usedPort = 0;

                // struct net_device *dev;
            	u8 desired_number_of_subflows = 0;

            	desired_number_of_subflows = nla_get_u8(nla);

            	// TODO pass net ! sinon ca foire
                // increments socket count so we need to release it 
                // Here we should lock devices but we conider they don't change


                // dev = first_net_device(&init_net);
                // while (dev) {
                //     meta_sk = mptcp_hash_find( dev, token );

                //     lig_debug(KERN_INFO "found device [%s]\n", dev->name);
                //     dev = next_net_device(dev);

                // }
            	
                // Here we should have a better optimiation

                meta_sk = mptcp_hash_find( 0, token );
                lig_debug("Ref count on meta sk after hash_find [%d]\n",meta_sk->sk_refcnt.counter);
            	

                if(!meta_sk){

            		lig_debug("Could not find meta_sk for token [%#x]\n", token);
            		return -1;
            	}

            	mpcb = tcp_sk(meta_sk)->mpcb;

            	// TODO ajuster le nb a créer en fct de celui déjà existant
            	// send a bitfield of 
            	mpcb->desired_number_of_subflows = max(mpcb->cnt_subflows,desired_number_of_subflows );

            	lig_debug("Desired nb of subflows set to %d\n", mpcb->desired_number_of_subflows );
			 	// mptcp_debug("Trying to set meta_sk port to its real value. (current value %d).\n",mpcb->locaddr4[0].port);
			 	// mpcb->locaddr4[0].port = ntohs(inet_sk(mpcb->meta_sk)->inet_sport);
			 	// mptcp_debug("New value: %d.\n",mpcb->locaddr4[0].port);

			 	mpcb->used_port_modulos = 0;
			 	usedPort = ntohs(inet_sk(mpcb->meta_sk)->inet_sport) ;
			 	mpcb->used_port_modulos |= (1 << (usedPort%  mpcb->desired_number_of_subflows) );

                lig_debug("Already used port=[%d]\n", usedPort);
			 	lig_debug("used port modulos set to [%d]\n", mpcb->used_port_modulos);

            	ndiffports_create_subflows(meta_sk);

                // atomic_dec_and_test()
                // a rempalcer par sock_put je crois
            }
            else
            {
                lig_debug("no info->attrs\n");
            }

            break;

        case ELC_REQUEST_RLOCS_FOR_EID:
            lig_debug(KERN_WARNING "recieved a priori a request for an eid : should not happen\n");

            return -1;

        case ELC_SET_MAP_RESOLVER:
            lig_debug(KERN_WARNING "recieved a priori a request ELC_SET_MAP_RESOLVER\n");

            return -1;

        default:
            lig_debug(KERN_WARNING "unknown command %d sent\n", cmd);
            return -1;
    }

    return 0;
}






