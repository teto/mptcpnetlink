#include <linux/module.h>

#include <net/mptcp.h>
#include <net/mptcp_v4.h>
#include <linux/genetlink.h>
#include <linux/inet.h>
#include <net/genetlink.h>


#include "mptcp_netlink.h"
#if IS_ENABLED(CONFIG_IPV6)
#include <net/mptcp_v6.h>
#endif


// Be able to override family name
// module_param(whom, charp, S_IRUGO);
// MODULE_PARM_DESC(programPath, "Path towards the userspace lig program this module calls ?");




struct genl_ops lig_genl_ops[ELC_MAX] = {

    {
        .cmd = ELC_REQUEST_RLOCS_FOR_EID,
        //.flags = 0,
        .policy = lig_policies,
        .doit = handle_results /* cb function should set this one or next but not both */
    } ,
    {
        .cmd = ELC_RESULTS,
        .policy = lig_policies,
        .doit = handle_results /* cb function */

    },
    {
        .cmd = ELC_SET_MAP_RESOLVER,
        .policy = lig_policies,
        .doit = handle_results /* cb function */

    }
};




// module_param_string(destinationIP, destinationIP, sizeof(destinationIP), 0644);
// module_param_string(programPath, programPath, sizeof(programPath), 0644);

// MODULE_PARM_DESC(resolverEID, "EID of the (proxy) map resolver");
// MODULE_PARM_DESC(hostEID, "EID of the running machine");


/** callback triggered on netlink message **/
int handle_results(struct sk_buff *skb_2, struct genl_info *info);




static int mptcp_get_port_modulo_n(int desired_port_rest_after_modulo, int modulo)
{
    int lowest_possible_port = 0, highest_possible_port = 0;
    uint16_t remaining = 0;
    uint16_t rover = 0;

    mptcp_debug("Looking for a port number with rest after modulo [%d] of [%d] (rest)\n", modulo,desired_port_rest_after_modulo);

    inet_get_local_port_range(&lowest_possible_port, &highest_possible_port);
    remaining = (highest_possible_port - lowest_possible_port) + 1;
    rover = ( net_random() % remaining ) + lowest_possible_port;
    rover += (desired_port_rest_after_modulo-(rover%modulo) );

    mptcp_debug("Try port number [%d]\n", rover);
    return rover;
}
 /* END */
// static int sysctl_mptcp_ndiffports __read_mostly = 2;

// int mptcp_update_used_modulos(struct mptcp_cb* mpcb) 
// {
//   int i = 0;
//   int desired_number_of_subflows = MPTCP_DESIRED_NB_OF_SUBFLOWS(mpcb);

//    mptcp_debug("Updating used port modulos \n"); //, mpcb->used_port_modulos);
 
//    // reset 
//    mpcb->used_port_modulos = 0;
//    mptcp_for_each_bit_set(mpcb->loc4_bits,i ){
 
//       /* if connection not established yet
//       don't take into account theport number */
//       if(mpcb->locaddr4[i].port == 0){
//            mptcp_debug("Skipping subflow: %d, connection looks not established yet\n", i);
//            continue;
//        }
 
//        mptcp_debug("%d port used for subflow : %d (rest %d)\n", mpcb->locaddr4[i].port , i , mpcb->locaddr4[i].port % desired_number_of_subflows);
//        modulo = mpcb->locaddr4[i].port  % desired_number_of_subflows;
       
//        // 1 << 0 should be 1
//        mpcb->used_port_modulos |= (1 << modulo );
//    }
 
//     mptcp_debug("to used port modulos %x (hexa)\n", mpcb->used_port_modulos);
//     return mpcb->used_port_modulos;
// }


static void on_session_establishment(struct sock *meta_sk)
{
    struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
    struct ndiffports_priv *pm_priv = (struct ndiffports_priv *)mpcb->mptcp_pm;

    // struct ndiffports_priv *pm_priv = (struct ndiffports_priv *)mpcb->mptcp_pm;
    // struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
    // struct sock *meta_sk = mpcb->meta_sk;




    if (!work_pending(&pm_priv->netlink_work)) {

        // pm_priv->subflow_work desired_number_of_subflows
        queue_work(mptcp_wq, &pm_priv->netlink_work);
    }


}


/**
 * Create all new subflows, by doing calls to mptcp_initX_subsockets
 *
 * This function uses a goto next_subflow, to allow releasing the lock between
 * new subflows and giving other processes a chance to do some work on the
 * socket and potentially finishing the communication.
 **/
 // , int sysctl_mptcp_ndiffports
void create_subflow_worker(struct work_struct *work)
{
    struct ndiffports_priv *pm_priv = container_of(work,
                             struct ndiffports_priv,
                             subflow_work);
    struct mptcp_cb *mpcb = pm_priv->mpcb;
    struct sock *meta_sk = mpcb->meta_sk;
    int iter = 0;
    int i = 0,desired_port_modulo = 0;
    /* TODO a renommer */
    int desired_number_of_subflows = mpcb->desired_number_of_subflows;
    
    // le récuperer dans le work_struct !
    // recupere le port de la meta_sk, on met a jour les modulos

    lig_debug("create_subflow_worker called. \n");
    lig_debug("Desired number of subflows [%d]. \n",desired_number_of_subflows);
    lig_debug("mpcb->used_port_modulos set to [%d]. \n",mpcb->used_port_modulos);

next_subflow:
    if (iter) {
        release_sock(meta_sk);
        mutex_unlock(&mpcb->mutex);

        yield();
    }
    mutex_lock(&mpcb->mutex);
    lock_sock_nested(meta_sk, SINGLE_DEPTH_NESTING);

    iter++;

    if (sock_flag(meta_sk, SOCK_DEAD))
        goto exit;

    if (mpcb->master_sk &&
        !tcp_sk(mpcb->master_sk)->mptcp->fully_established)
        goto exit;



    if (desired_number_of_subflows > iter &&
        desired_number_of_subflows > mpcb->cnt_subflows) {

        /* looking for 
   mptcp_update_used_modulos(mpcb);
   compute a desired port modulo */
           mptcp_for_each_bit_unset( mpcb->used_port_modulos, i ){
         
                /* only want modulos < number_of_subflows once we found it we exit the loop 
                That's a bit confident since we are not sure the bind will succeed but as we don't keep trace of 
                locators in this module
                */

                if(i < desired_number_of_subflows){
                   desired_port_modulo = i;
                   lig_debug("Desired port modulo [%d]\n",desired_port_modulo);
                   mpcb->used_port_modulos |= (1 <<  desired_port_modulo );
                    lig_debug("New used port modulo set to [%d]\n",mpcb->used_port_modulos);
                   break;
                }
         
           }

        if (meta_sk->sk_family == AF_INET ||
            mptcp_v6_is_v4_mapped(meta_sk)) {
            struct mptcp_loc4 loc;

            loc.addr.s_addr = inet_sk(meta_sk)->inet_saddr;
            loc.id = 0;
            loc.low_prio = 0;

            // TODO generer le port a partir du desired_number_of_subflows > mpcb->cnt_subflows
            // ajouter la fct de génération du port
            // Generate a good port number
            loc.port = htons( mptcp_get_port_modulo_n( desired_port_modulo , desired_number_of_subflows) );
            // lig_debug("Attempting connection towards  ");
            // TODO in the server case I do not want to init the socket but rather to advertise
            mptcp_init4_subsockets(meta_sk, &loc, &mpcb->remaddr4[0]);
        } else {

#if IS_ENABLED(CONFIG_IPV6)
            lig_debug("Compile without IPv6, not supported. \n");
            // struct mptcp_loc6 loc;

            // loc.addr = inet6_sk(meta_sk)->saddr;
            // loc.id = 0;
            // loc.low_prio = 0;

            // mptcp_init6_subsockets(meta_sk, &loc, &mpcb->remaddr6[0]);
#endif
        }
        goto next_subflow;
    }

exit:
    release_sock(meta_sk);
    mutex_unlock(&mpcb->mutex);
    lig_debug("Ref count on meta sk before sock_put [%d]\n",meta_sk->sk_refcnt.counter);
    sock_put(meta_sk); // ptet en rajouter un 
}

static void ndiffports_new_session(struct sock *meta_sk, u8 id)
{
    struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
    struct ndiffports_priv *fmp = (struct ndiffports_priv *)mpcb->mptcp_pm;

    /* Initialize workqueue-struct */
    INIT_WORK(&fmp->subflow_work, create_subflow_worker);
    INIT_WORK(&fmp->netlink_work, send_request_for_eid);
    fmp->mpcb = mpcb;
}

void ndiffports_create_subflows(struct sock *meta_sk)
{
    struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
    struct ndiffports_priv *pm_priv = (struct ndiffports_priv *)mpcb->mptcp_pm;

    if (mpcb->infinite_mapping_snd || mpcb->infinite_mapping_rcv ||
        mpcb->send_infinite_mapping ||
        mpcb->server_side || sock_flag(meta_sk, SOCK_DEAD))
        return;

    if (!work_pending(&pm_priv->subflow_work)) {
        // Plu besoin de ca puiqu'on a deja capture le truc via
        // mptcp_find_hash
        //sock_hold(meta_sk);

        queue_work(mptcp_wq, &pm_priv->subflow_work);
    }
}

static int ndiffports_get_local_id(sa_family_t family, union inet_addr *addr,
                  struct net *net)
{
    return 0;
}

static struct mptcp_pm_ops ndiffports __read_mostly = {
    .new_session = ndiffports_new_session,
    .fully_established = on_session_establishment,  // ndiffports_create_subflows
    .get_local_id = ndiffports_get_local_id,
    // .addr_signal
    // .new_remote_address

    .name = "netlink",
    .owner = THIS_MODULE,
};



/* General initialization of MPTCP_PM */
static int __init ndiffports_register(void)
{
    int rc = 0;

    BUILD_BUG_ON(sizeof(struct ndiffports_priv) > MPTCP_PM_SIZE);



    lig_debug("LIG MODULE initialization\n");

    /*register new family*/
    rc = genl_register_family_with_ops(&lig_gnl_family,lig_genl_ops, ARRAY_SIZE(lig_genl_ops) );
    if (rc != 0){
        lig_debug( "could not register ops: %i\n",rc);
        goto failure;
    }



    //
    rc = genl_register_mc_group(&lig_gnl_family, &lig_multicast_group);

    if(rc != 0)
    {
        lig_debug( "could not register multicast group: %d\n",rc);
        goto failure;
    }
    lig_debug( "Successuflly registered to family: %s\n", LIG_FAMILY_NAME);

    if (mptcp_register_path_manager(&ndiffports)){
        lig_debug( "could not register multicast group: %d\n",rc);
        return -1;
    }


    return 0;

failure:
    lig_debug(KERN_ERR "an error occured while inserting the generic netlink example module\n");
    return -1;

}

static void ndiffports_unregister(void)
{

    int ret = 0;
    mptcp_unregister_path_manager(&ndiffports);




    lig_debug(KERN_INFO "cleanup_lig_module() called\n");

    ret = genl_unregister_family(&lig_gnl_family);
    if( ret != 0)
    {
        lig_debug(KERN_WARNING "Could not unregister family, error: %d\n", ret);
    }

}

module_init(ndiffports_register);
module_exit(ndiffports_unregister);

MODULE_AUTHOR("Matthieu Coudron");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Netlink MPTCP");
MODULE_VERSION("0.88");
